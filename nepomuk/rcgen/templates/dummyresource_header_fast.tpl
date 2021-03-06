/*
 * This file is part of the Nepomuk KDE project.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

/*
 * This file has been generated by the Nepomuk Resource class generator.
 * DO NOT EDIT THIS FILE.
 * ANY CHANGES WILL BE LOST.
 */

#ifndef _NEPOMUK_FAST_RESOURCE_H_
#define _NEPOMUK_FAST_RESOURCE_H_

#include <soprano/node.h>

#include <QtCore/QUrl>

namespace NepomukFast {

    class Resource
    {
    public:
        QUrl uri() const;
        QUrl graphUri() const;
        QUrl type() const;

        void addProperty( const QUrl &uri, const Soprano::Node &node );
        void setLabel( const QString &label );

    protected:
        Resource( const QUrl& uri, const QUrl& graphUri, const QUrl& type );

    private:
        QUrl m_uri;
        QUrl m_graphUri;
        QUrl m_type;
    };
}

#endif
