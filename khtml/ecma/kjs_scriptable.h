/* This file is part of the KDE project
   Copyright (C) 2010 Maksim Orlovich <maksim@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/


#ifndef KJS_SCRIPTABLE_H
#define KJS_SCRIPTABLE_H

#include <khtml_part.h>
#include <kparts/scriptableextension.h>
#include <kjs/object.h>
#include <kjs/list.h>
#include <QHash>

using namespace KParts;

namespace KJS {

class WrapScriptableObject;

class KHTMLScriptable: public ScriptableExtension
{
public:
    KHTMLScriptable(KHTMLPart* part);


    // ScriptableExtension API
    virtual QVariant rootObject();
    virtual QVariant enclosingObject(KParts::ReadOnlyPart* childPart);
    virtual QVariant callAsFunction(ScriptableExtension* callerPrincipal, quint64 objId, const ArgList& args);
    virtual QVariant callFunctionReference(ScriptableExtension* callerPrincipal, quint64 objId,
                                           const QString& f, const ArgList& args);
    virtual QVariant callAsConstructor(ScriptableExtension* callerPrincipal, quint64 objId, const ArgList& args);
    virtual bool hasProperty(ScriptableExtension* callerPrincipal, quint64 objId, const QString& propName);
    virtual QVariant get(ScriptableExtension* callerPrincipal, quint64 objId, const QString& propName);
    virtual bool put(ScriptableExtension* callerPrincipal, quint64 objId, const QString& propName, const QVariant& value);
    virtual bool removeProperty(ScriptableExtension* callerPrincipal, quint64 objId, const QString& propName);
    virtual bool enumerateProperties(ScriptableExtension* callerPrincipal, quint64 objId, QStringList* result);

    virtual bool setException(ScriptableExtension* callerPrincipal, const QString& message);
    virtual QVariant evaluateScript(ScriptableExtension* callerPrincipal,
                                    quint64 contextObjectId,
                                    const QString& code,
                                    const QString& language);
    virtual void acquire(quint64 objid);
    virtual void release(quint64 objid);

public:
    // We keep a weak table of our wrappers for external objects, so that
    // the same object gets the same wrapper all the time.
    static QHash<Object,      WrapScriptableObject*>* importedObjects();
    static QHash<FunctionRef, WrapScriptableObject*>* importedFunctions();

    static JSValue* importValue(ExecState* exec, const QVariant& v);
    static JSValue* importFunctionRef(ExecState* exec, const QVariant& v);
    static JSObject* importObject(ExecState* exec, const QVariant& v);

    static QVariant exportValue (JSValue* v);
    static QVariant exportObject(JSValue* v);
private:
    // May return 0. Used for security checks!
    KHTMLPart* partForPrincipal(ScriptableExtension* callerPrincipal);

    // May return null.
    JSObject* objectForId(quint64 objId);

    List decodeArgs(const ArgList& args);


    QVariant exception(const char* msg);
    QVariant scriptableNull();

    // If the given object is owned by a KHTMLScriptable, return the
    // JS object for it. If not, return 0.
    static JSObject* tryGetNativeObject(const Object& sObj);

    static QHash<Object,      WrapScriptableObject*>* s_importedObjects;
    static QHash<FunctionRef, WrapScriptableObject*>* s_importedFunctions;
    KHTMLPart* m_part;

};

// This represents an object we imported from a foreign ScriptableExtension
class WrapScriptableObject: public JSObject {
public:
    enum Type {
        Object,
        FunctionRef
    };

    WrapScriptableObject(ExecState* exec, Type t,
                         ScriptableExtension* owner, quint64 objId,
                         const QString& field = QString());

   ~WrapScriptableObject();

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int);
    virtual bool deleteProperty(ExecState* exec, const Identifier& i);

    virtual bool isFunctionType() const { return false; }
    virtual bool implementsCall() const { return true; }
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    // We claim true, since may be calleable
    virtual bool implementsConstruct() const { return true; }
    virtual JSObject* construct(ExecState* exec, const List& args);

    virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&);

    virtual UString toString(ExecState *exec) const;
private:
    // If we're a function reference type, before we perform non-call operations we need
    // to actually lookup the field. This takes care of that.
    ScriptableExtension::Object resolveAnyReferences(ExecState* exec, bool* ok);


    // resolves all function references to get an ref, if any.
    ScriptableExtension::Object resolveReferences(ExecState* exec,
                                                  const ScriptableExtension::FunctionRef& f,
                                                  bool* ok);

    // gets a field of the given object id, base
    QVariant doGet(ExecState* exec, const ScriptableExtension::Object& o,
                   const QString& field, bool* ok);

    ScriptableExtension::ArgList exportArgs(const List& l);

    // Looks up the principal we're running as
    ScriptableExtension* principal(ExecState* exec);

    // what we wrap
    QWeakPointer<ScriptableExtension> objExtension;
    quint64 objId;
    QString field;
    Type    type;

    // this is an unguarded copy of objExtension. We need it in order to
    // clean ourselves up from the imports tables properly even if the peer
    // was destroyed.
    ScriptableExtension* tableKey;
};

}

#endif
// kate: space-indent on; indent-width 4; replace-tabs on;