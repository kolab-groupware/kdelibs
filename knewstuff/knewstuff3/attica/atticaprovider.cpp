/*
    Copyright (c) 2009 Frederik Gladhorn <gladhorn@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "atticaprovider.h"

#include "xmlloader.h"
#include "core/provider_p.h"

#include <kdebug.h>
#include <klocale.h>
#include <kio/job.h>

#include <attica/providermanager.h>
#include <attica/provider.h>
#include <attica/listjob.h>
#include <attica/content.h>

using namespace Attica;

namespace KNS3
{

class AtticaProviderPrivate :public ProviderPrivate
{
public:
    QString m_categories;
    Attica::ProviderManager m_providerManager;
    Attica::Provider m_provider;
    
    AtticaProviderPrivate()
    {
        // FIXME maybe we want something that is not in the category Wallpaper 640 by 480 at some point
        // the categories can be gotten from the provider
        m_categories = QLatin1String("175");
    }
};

AtticaProvider::AtticaProvider()
    : Provider(*new AtticaProviderPrivate)
{
    // TODO
    Q_D(AtticaProvider);
    d->mName = KTranslatable("Attica");

    connect(&d->m_providerManager, SIGNAL(providersChanged()), SLOT(providerLoaded()));
}

AtticaProvider::~AtticaProvider()
{
    // d_ptr is deleted in base class!
}

bool AtticaProvider::setProviderXML(QDomElement & xmldata)
{
    Q_D(AtticaProvider);
    kDebug(550) << "setting provider xml";

    if (xmldata.tagName() != "provider")
        return false;
    
    // FIXME this is quite ugly, repackaging the xml into a string
    QDomDocument doc("temp");
    doc.appendChild(xmldata);
    d->m_providerManager.addProviderFromXml(doc.toString());

    if (!d->m_providerManager.providers().isEmpty()) {
        kDebug() << "base url of attica provider:" << d->m_providerManager.providers().first().baseUrl().toString();
    }
    
    if (d->m_providerManager.providers().isEmpty()) {
        return false;
    }
    return true;
}

QDomElement AtticaProvider::providerXML() const
{
    Q_D(const AtticaProvider);
    QDomDocument doc;

    QDomElement el = doc.createElement("provider");
    // FIXME if needed, create xml
    return el;
}

void AtticaProvider::providerLoaded()
{
    Q_D(AtticaProvider);
    if (d->m_providerManager.providers().isEmpty()) {
        return;
    }
    d->m_provider = d->m_providerManager.providers().first();
    emit providerInitialized(this);
}

bool AtticaProvider::isInitialized() const
{
    Q_D(const AtticaProvider);
    return !d->m_providerManager.providers().isEmpty();
}

QStringList AtticaProvider::availableSortingCriteria() const
{
    // URL Arguments: sortmode - The sortmode of the list. Possible values are: "new" - newest first , "alpha" - alphabetical, "high" - highest rated, "down" - most downloads
    return QStringList() << I18N_NOOP("Date") << I18N_NOOP("Alphabetical") << I18N_NOOP("Highest Rating") << I18N_NOOP("Most Downloads");
}

void AtticaProvider::loadEntries(const QString& sortMode, const QString& searchString, int page, int pageSize)
{
    Q_D(AtticaProvider);

    Attica::Provider::SortMode sorting = sortModeFromString(sortMode);
    Attica::Category::List cats;
    Attica::Category cat;
    cat.setId(d->m_categories);
    cats.append(cat);
    ListJob<Content>* job = d->m_provider.searchContents(cats, searchString, sorting, page, pageSize);
    connect(job, SIGNAL(finished(Attica::BaseJob*)), SLOT(categoryContentsLoaded(Attica::BaseJob*)));
    job->start();
}

void AtticaProvider::categoryContentsLoaded(BaseJob* job)
{
    Q_D(AtticaProvider);
    ListJob<Content>* listJob = static_cast<ListJob<Content>*>(job);
    Content::List contents = listJob->itemList();

    Entry::List entries;
    
    Q_FOREACH(Content content, contents) {
        Entry entry;
        entry.setName(content.name());
        entry.setUniqueId(content.id());
        entry.setProviderId(d->m_provider.baseUrl().toString());
        entry.setRating(content.rating());
        entry.setDownloads(content.downloads());
        entry.setReleaseDate(content.updated().date());
        entry.setPreview(content.previewPicture("1"));
        entry.setLicense(content.license());
        //entry.setAuthor(content.author());
        entry.setSource(KNS3::Entry::Online);
        entry.setStatus(KNS3::Entry::Downloadable);
        entry.setSummary(content.description());
        entry.setVersion(content.version());
        
        entries.append(entry);
    }

    // FIXME page number and strings
    emit loadingFinished("", "", 0, entries.count(), 10, entries);
}

Attica::Provider::SortMode AtticaProvider::sortModeFromString(const QString& sortString)
{
    if (sortString == "Date") {
        return Attica::Provider::Newest;
    }
    if (sortString == "Alphabetical") {
        return Attica::Provider::Alphabetical;
    }
    if (sortString == "Most Downloads") {
        return Attica::Provider::Downloads;
    }
    return Attica::Provider::Rating;
}


}

#include "atticaprovider.moc"