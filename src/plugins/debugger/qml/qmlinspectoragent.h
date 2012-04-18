/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef QMLINSPECTORAGENT_H
#define QMLINSPECTORAGENT_H

#include <QObject>

#include <qmldebug/baseenginedebugclient.h>
#include <watchdata.h>

using namespace QmlDebug;

namespace Debugger {

class DebuggerEngine;

namespace Internal {

class WatchData;

//map <filename, editorRevision> -> <lineNumber, columnNumber> -> debugId
typedef
QHash<QPair<QString, int>, QHash<QPair<int, int>, QList<int> > > DebugIdHash;

class QmlInspectorAgent : public QObject
{
    Q_OBJECT
public:
    explicit QmlInspectorAgent(DebuggerEngine *engine, QObject *parent = 0);


    void refreshObjectTree();
    void fetchObject(int debugId);
    quint32 queryExpressionResult(int debugId, const QString &expression);

    void updateWatchData(const WatchData &data);
    void selectObjectInTree(int debugId);

    quint32 setBindingForObject(int objectDebugId,
                                const QString &propertyName,
                                const QVariant &value,
                                bool isLiteralValue,
                                QString source,
                                int line);
    quint32 setMethodBodyForObject(int objectDebugId, const QString &methodName,
                                   const QString &methodBody);
    quint32 resetBindingForObject(int objectDebugId,
                                  const QString &propertyName);

    QList<QmlDebugObjectReference> objects() const;
    QmlDebugObjectReference objectForId(int debugId) const;
    QmlDebugObjectReference objectForId(const QString &objectId) const;
    QmlDebugObjectReference objectForLocation(int line, int column) const;
    QList<QmlDebugObjectReference> rootObjects() const { return m_rootObjects; }
    DebugIdHash debugIdHash() const { return m_debugIdHash; }

    bool addObjectWatch(int objectDebugId);
    bool isObjectBeingWatched(int objectDebugId);
    bool removeObjectWatch(int objectDebugId);
    void removeAllObjectWatches();

    void setEngineClient(BaseEngineDebugClient *client);

signals:
    void objectTreeUpdated();
    void objectFetched(const QmlDebugObjectReference &ref);
    void expressionResult(quint32 queryId, const QVariant &value);
    void propertyChanged(int debugId, const QByteArray &propertyName,
                         const QVariant &propertyValue);

private slots:
    void updateStatus();
    void onResult(quint32 queryId, const QVariant &value, const QByteArray &type);
    void newObjects();

private:
    void reloadEngines();
    void queryEngineContext(int id);
    quint32 fetchContextObject(const QmlDebugObjectReference &obj);
    void fetchRootObjects(const QmlDebugContextReference &context, bool clear);

    void updateEngineList(const QmlDebugEngineReferenceList &engines);
    void rootContextChanged(const QmlDebugContextReference &context);
    void objectTreeFetched(const QmlDebugObjectReference &result);
    void onCurrentObjectsFetched(const QmlDebugObjectReference &result);
    bool getObjectHierarchy(const QmlDebugObjectReference &object);


    void buildDebugIdHashRecursive(const QmlDebugObjectReference &ref);
    QList<WatchData> buildWatchData(const QmlDebugObjectReference &obj,
                                           const WatchData &parent);
    void addObjectToTree(const QmlDebugObjectReference &obj, bool notify);

    QmlDebugObjectReference objectForId(
            int debugId,
            const QmlDebugObjectReference &ref) const;
    QList<QmlDebugObjectReference> objects(
            const QmlDebugObjectReference &objectRef) const;


    enum LogDirection {
        LogSend,
        LogReceive
    };
    void log(LogDirection direction, const QString &message);

    bool isConnected();

private:
    DebuggerEngine *m_engine;
    QmlDebug::BaseEngineDebugClient *m_engineClient;

    quint32 m_engineQueryId;
    quint32 m_rootContextQueryId;
    int m_objectToSelect;
    QList<quint32> m_objectTreeQueryIds;
    QList<QmlDebugObjectReference> m_rootObjects;
    QList<quint32> m_fetchCurrentObjectsQueryIds;
    QList<QmlDebugObjectReference> m_fetchCurrentObjects;
    QmlDebugEngineReferenceList m_engines;
    QHash<int, QByteArray> m_debugIdToIname;
    DebugIdHash m_debugIdHash;

    QList<int> m_objectWatches;
};

} // Internal
} // Debugger

#endif // QMLINSPECTORAGENT_H
