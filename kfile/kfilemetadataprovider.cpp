/*****************************************************************************
 * Copyright (C) 2010 by Peter Penz <peter.penz@gmx.at>                      *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License as published by the Free Software Foundation; either              *
 * version 2 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

#include "kfilemetadataprovider_p.h"

#include <kfileitem.h>
#include "knfotranslator_p.h"
#include <klocale.h>
#include <kurl.h>

#include <config-nepomuk.h>
#ifdef HAVE_NEPOMUK
    #define DISABLE_NEPOMUK_LEGACY
    #include <nepomukmassupdatejob.h>
    #include "kratingwidget.h"
    #include <resource.h>
    #include <resourcemanager.h>

    #include "kcommentwidget_p.h"
    #include "kloadfilemetadatathread_p.h"
    #include "ktaggingwidget_p.h"
#else
    namespace Nepomuk
    {
        typedef int Tag;
    }
#endif

class KFileMetaDataProvider::Private
{

public:
    Private(KFileMetaDataProvider* parent);
    ~Private();

    void slotLoadingFinished(QThread* finishedThread);

    void slotRatingChanged(unsigned int rating);
    void slotTagsChanged(const QList<Nepomuk::Tag>& tags);
    void slotCommentChanged(const QString& comment);

    void slotMetaDataUpdateDone();
    void slotTagActivated(const Nepomuk::Tag& tag);

    /**
     * Disables the metadata widget and starts the job that
     * changes the meta data asynchronously. After the job
     * has been finished, the metadata widget gets enabled again.
     */
    void startChangeDataJob(KJob* job);

#ifdef HAVE_NEPOMUK
    QList<Nepomuk::Resource> resourceList() const;
#endif

    bool m_readOnly;
    bool m_nepomukActivated;
    QList<KFileItem> m_fileItems;

#ifdef HAVE_NEPOMUK
    QHash<KUrl, Nepomuk::Variant> m_data;

    QList<KLoadFileMetaDataThread*> m_metaDataThreads;
    KLoadFileMetaDataThread* m_latestMetaDataThread;

    KRatingWidget* m_ratingWidget;
    KTaggingWidget* m_taggingWidget;
    KCommentWidget* m_commentWidget;
#endif

private:
    KFileMetaDataProvider* const q;
};

KFileMetaDataProvider::Private::Private(KFileMetaDataProvider* parent) :
    m_readOnly(false),
    m_nepomukActivated(false),
    m_fileItems(),
#ifdef HAVE_NEPOMUK
    m_data(),
    m_metaDataThreads(),
    m_latestMetaDataThread(0),
    m_ratingWidget(0),
    m_taggingWidget(0),
    m_commentWidget(0),
#endif
    q(parent)
{
#ifdef HAVE_NEPOMUK
    m_nepomukActivated = (Nepomuk::ResourceManager::instance()->init() == 0);
    if (m_nepomukActivated) {
        m_ratingWidget = new KRatingWidget();
        // TODO:
        //m_ratingWidget->setFixedHeight(fontMetrics.height());
        //const Qt::Alignment align = (parent->layoutDirection() == Qt::LeftToRight) ?
        //                           Qt::AlignLeft : Qt::AlignRight;
        //m_ratingWidget->setAlignment(align);
        connect(m_ratingWidget, SIGNAL(ratingChanged(unsigned int)),
                q, SLOT(slotRatingChanged(unsigned int)));
        m_ratingWidget->setVisible(false);

        m_taggingWidget = new KTaggingWidget();
        connect(m_taggingWidget, SIGNAL(tagsChanged(const QList<Nepomuk::Tag>&)),
                q, SLOT(slotTagsChanged(const QList<Nepomuk::Tag>&)));
        connect(m_taggingWidget, SIGNAL(tagActivated(const Nepomuk::Tag&)),
                q, SLOT(slotTagActivated(const Nepomuk::Tag&)));
        m_taggingWidget->setVisible(false);

        m_commentWidget = new KCommentWidget();
        connect(m_commentWidget, SIGNAL(commentChanged(const QString&)),
                q, SLOT(slotCommentChanged(const QString&)));
        m_commentWidget->setVisible(false);
    }
#endif
}

KFileMetaDataProvider::Private::~Private()
{
#ifdef HAVE_NEPOMUK
    delete m_ratingWidget;
    delete m_taggingWidget;
    delete m_commentWidget;

    foreach (KLoadFileMetaDataThread* thread, m_metaDataThreads) {
        disconnect(thread, SIGNAL(finished(QThread*)),
                   q, SLOT(slotLoadingFinished(QThread*)));
        thread->wait();
    }
#endif
}

void KFileMetaDataProvider::Private::slotLoadingFinished(QThread* finishedThread)
{
#ifdef HAVE_NEPOMUK
    // The thread that has emitted the finished() signal
    // will get deleted and removed from m_metaDataThreads.
    const int threadsCount = m_metaDataThreads.count();
    for (int i = 0; i < threadsCount; ++i) {
        KLoadFileMetaDataThread* thread = m_metaDataThreads[i];
        if (thread == finishedThread) {
            m_metaDataThreads.removeAt(i);
            if (thread != m_latestMetaDataThread) {
                // Ignore data of older threads, as the data got
                // obsolete by m_latestMetaDataThread.
                thread->deleteLater();
                return;
            }
        }
    }

    m_data = m_latestMetaDataThread->data();
    m_latestMetaDataThread->deleteLater();
#else
    Q_UNUSED(finishedThread)
#endif

    emit q->loadingFinished();
}


void KFileMetaDataProvider::Private::slotRatingChanged(unsigned int rating)
{
#ifdef HAVE_NEPOMUK
    Nepomuk::MassUpdateJob* job =
        Nepomuk::MassUpdateJob::rateResources(resourceList(), rating);
    startChangeDataJob(job);
#else
    Q_UNUSED(rating);
#endif
}

void KFileMetaDataProvider::Private::slotTagsChanged(const QList<Nepomuk::Tag>& tags)
{
#ifdef HAVE_NEPOMUK
    m_taggingWidget->setTags(tags);

    Nepomuk::MassUpdateJob* job =
        Nepomuk::MassUpdateJob::tagResources(resourceList(), tags);
    startChangeDataJob(job);
#else
    Q_UNUSED(tags);
#endif
}

void KFileMetaDataProvider::Private::slotCommentChanged(const QString& comment)
{
#ifdef HAVE_NEPOMUK
    Nepomuk::MassUpdateJob* job =
        Nepomuk::MassUpdateJob::commentResources(resourceList(), comment);
    startChangeDataJob(job);
#else
    Q_UNUSED(comment);
#endif
}

void KFileMetaDataProvider::Private::slotTagActivated(const Nepomuk::Tag& tag)
{
#ifdef HAVE_NEPOMUK
    emit q->urlActivated(tag.resourceUri());
#else
    Q_UNUSED(tag);
#endif
}

void KFileMetaDataProvider::Private::startChangeDataJob(KJob* job)
{
    connect(job, SIGNAL(result(KJob*)),
            q, SIGNAL(dataChangeFinished()));
    emit q->dataChangeStarted();
    job->start();
}

#ifdef HAVE_NEPOMUK
QList<Nepomuk::Resource> KFileMetaDataProvider::Private::resourceList() const
{
    QList<Nepomuk::Resource> list;
    foreach (const KFileItem& item, m_fileItems) {
        const KUrl url = item.url();
        list.append(Nepomuk::Resource(url));
    }
    return list;
}
#endif

KFileMetaDataProvider::KFileMetaDataProvider(QObject* parent) :
    QObject(parent),
    d(new Private(this))
{
}

KFileMetaDataProvider::~KFileMetaDataProvider()
{
    delete d;
}

void KFileMetaDataProvider::setItems(const KFileItemList& items)
{
    d->m_fileItems = items;

#ifdef HAVE_NEPOMUK
    if (items.isEmpty()) {
        return;
    }

    QList<KUrl> urls;
    foreach (const KFileItem& item, items) {
        const KUrl url = item.nepomukUri();
        if (url.isValid()) {
            urls.append(url);
        }
    }

    // Cancel all threads that have not emitted a finished() signal.
    // The deleting of those threads is done in slotLoadingFinished().
    foreach (KLoadFileMetaDataThread* thread, d->m_metaDataThreads) {
        thread->cancel();
    }

    // create a new thread that will provide the meeta data for the items
    d->m_latestMetaDataThread = new KLoadFileMetaDataThread();
    connect(d->m_latestMetaDataThread, SIGNAL(finished(QThread*)),
            this, SLOT(slotLoadingFinished(QThread*)));
    d->m_latestMetaDataThread->load(urls);
    d->m_metaDataThreads.append(d->m_latestMetaDataThread);
#endif
}

QString KFileMetaDataProvider::label(const KUrl& metaDataUri) const
{
    QString label;
    const QString uri = metaDataUri.url();
    if (uri == QLatin1String("kfileitem#rating")) {
        label = i18nc("@label", "Rating");
    } else if (uri == QLatin1String("kfileitem#tags")) {
        label = i18nc("@label", "Tags");
    } else if (uri == QLatin1String("kfileitem#comment")) {
        label = i18nc("@label", "Comment");
    } else {
        label = KNfoTranslator::instance().translation(metaDataUri);
    }

    return label;
}

QString KFileMetaDataProvider::group(const KUrl& metaDataUri) const
{
    QString group; // return value

    const QString uri = metaDataUri.url();
    if (uri == QLatin1String("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#width")) {
        group = QLatin1String("0sizeA");
    } else if (uri == QLatin1String("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#height")) {
        group = QLatin1String("0sizeB");
    }

    return group;
}

KFileItemList KFileMetaDataProvider::items() const
{
    return d->m_fileItems;
}

void KFileMetaDataProvider::setReadOnly(bool readOnly)
{
    if (readOnly != d->m_readOnly) {
        d->m_readOnly = readOnly;

#ifdef HAVE_NEPOMUK
        if (d->m_taggingWidget != 0) {
            d->m_taggingWidget->setReadOnly(readOnly);
        }
        if (d->m_commentWidget != 0) {
            d->m_commentWidget->setReadOnly(readOnly);
        }
#endif

        readOnlyChanged(readOnly);
    }
}

bool KFileMetaDataProvider::isReadOnly() const
{
    return d->m_readOnly;
}

#ifdef HAVE_NEPOMUK
QHash<KUrl, Nepomuk::Variant> KFileMetaDataProvider::data() const
{
    return d->m_data;
}

QWidget* KFileMetaDataProvider::valueWidget(const KUrl& metaDataUri) const
{
    QWidget* widget = 0;

    if (d->m_nepomukActivated) {
        const QString uri = metaDataUri.url();
        if (uri == QLatin1String("kfileitem#rating")) {
            widget = d->m_ratingWidget;
        } else if (uri == QLatin1String("kfileitem#tags")) {
            widget = d->m_taggingWidget;
        } else if (uri == QLatin1String("kfileitem#comment")) {
            widget = d->m_commentWidget;
        }
    }

    return widget;
}

bool KFileMetaDataProvider::setValue(const KUrl& metaDataUri, const Nepomuk::Variant& value)
{
    if (d->m_nepomukActivated) {
        QWidget* widget = valueWidget(metaDataUri);
        if (widget == d->m_ratingWidget) {
            d->m_ratingWidget->setRating(value.toInt());
            return true;
        }

        if (widget == d->m_taggingWidget) {
            QList<Nepomuk::Variant> variants = value.toVariantList();
            QList<Nepomuk::Tag> tags;
            foreach (const Nepomuk::Variant& variant, variants) {
                const Nepomuk::Resource resource = variant.toResource();
                tags.append(static_cast<Nepomuk::Tag>(resource));
            }
            d->m_taggingWidget->setTags(tags);
            return true;
        }

        if (widget == d->m_commentWidget) {
            d->m_commentWidget->setText(value.toString());
            return true;
        }
    }
    return false;
}
#endif

void KFileMetaDataProvider::readOnlyChanged(bool readOnly)
{
    Q_UNUSED(readOnly);
}

#include "kfilemetadataprovider_p.moc"