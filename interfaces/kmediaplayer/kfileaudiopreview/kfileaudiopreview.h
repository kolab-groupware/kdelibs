/* This file is part of the KDE libraries
   Copyright (C) 2003 Carsten Pfeiffer <pfeiffer@kde.org>
   Copyright (C) 2006 Matthias Kretz <kretz@kde.org>

   library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation, version 2.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KFILEAUDIOPREVIEW_H
#define KFILEAUDIOPREVIEW_H

#include <kurl.h>
#include <kpreviewwidgetbase.h>
#include <phonon/phononnamespace.h>

class QCheckBox;


/**
 * Audio "preview" widget for the file dialog.
 */
class KFileAudioPreview : public KPreviewWidgetBase
{
    Q_OBJECT

public:
    //TODO: make explicit in KDE5
    KFileAudioPreview( QWidget *parent = 0, //krazy:exclude=explicit until KDE5
                       const QVariantList &args = QVariantList() );
    ~KFileAudioPreview();

public Q_SLOTS:
    virtual void showPreview(const KUrl &url);
    virtual void clearPreview();

private Q_SLOTS:
    void toggleAuto( bool );
    void stateChanged( Phonon::State, Phonon::State );

private:
    QCheckBox *m_autoPlay;

private:
    class Private;
    Private *d;
};

#endif // KFILEAUDIOPREVIEW_H
