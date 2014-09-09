/*
    Copyright (c) 2014 Christian Mollekopf <mollekopf@kolabsys.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/


#include <qtest_kde.h>

#include <krecursivefilterproxymodel.h>
#include <QStandardItemModel>

class ModelSignalSpy : public QObject {
    Q_OBJECT
public:
    explicit ModelSignalSpy(QAbstractItemModel &model) {
        connect(&model, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(onRowsInserted(QModelIndex,int,int)));
        connect(&model, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(onRowsRemoved(QModelIndex,int,int)));
        connect(&model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)), this, SLOT(onRowsMoved(QModelIndex,int,int, QModelIndex, int)));
        connect(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(onDataChanged(QModelIndex,QModelIndex)));
        connect(&model, SIGNAL(layoutChanged()), this, SLOT(onLayoutChanged()));
        connect(&model, SIGNAL(modelReset()), this, SLOT(onModelReset()));
    }

    QStringList mSignals;
    QModelIndex parent;
    int start;
    int end;

public Q_SLOTS:
    void onRowsInserted(QModelIndex p, int s, int e) {
        mSignals << QLatin1String("rowsInserted");
        parent = p;
        start = s;
        end = e;
    }
    void onRowsRemoved(QModelIndex p, int s, int e) {
        mSignals << QLatin1String("rowsRemoved");
        parent = p;
        start = s;
        end = e;
    }
    void onRowsMoved(QModelIndex,int,int,QModelIndex,int) {
        mSignals << QLatin1String("rowsMoved");
    }
    void onDataChanged(QModelIndex,QModelIndex) {
        mSignals << QLatin1String("dataChanged");
    }
    void onLayoutChanged() {
        mSignals << QLatin1String("layoutChanged");
    }
    void onModelReset() {
        mSignals << QLatin1String("modelReset");
    }
};

class TestModel : public KRecursiveFilterProxyModel
{
    Q_OBJECT
public:
    virtual bool acceptRow(int sourceRow, const QModelIndex &sourceParent) const
    {
        // qDebug() << sourceModel()->index(sourceRow, 0, sourceParent).data().toString() << sourceModel()->index(sourceRow, 0, sourceParent).data(Qt::UserRole+1).toBool();
        return sourceModel()->index(sourceRow, 0, sourceParent).data(Qt::UserRole+1).toBool();
    }
};

static QModelIndex getIndex(char *string, const QAbstractItemModel &model)
{
    QModelIndexList list = model.match(model.index(0, 0), Qt::DisplayRole, QString::fromLatin1(string), 1, Qt::MatchRecursive);
    if (list.isEmpty()) {
        return QModelIndex();
    }
    return list.first();
}

class KRecursiveFilterProxyModelTest : public QObject
{
    Q_OBJECT
private:

private slots:
    // Requires the acceptRow fix in sourceDataChanged to pass
    // Test that we properly react to a data-changed signal in a descendant and include all required rows
    void testDataChange()
    {
        QStandardItemModel model;
        TestModel proxy;
        proxy.setSourceModel(&model);

        QStandardItem *row1 = new QStandardItem("row1");
        row1->setData(false);
        model.appendRow(row1);

        QCOMPARE(getIndex("row1", proxy).isValid(), false);

        QStandardItem *subchild = new QStandardItem("subchild");
        subchild->setData(false);
        {
            QStandardItem *child = new QStandardItem("child");
            child->setData(false);
            child->appendRow(subchild);
            row1->appendRow(child);
        }

        ModelSignalSpy spy(proxy);
        subchild->setData(true);

        QCOMPARE(getIndex("row1", proxy).isValid(), true);
        QCOMPARE(getIndex("child", proxy).isValid(), true);
        QCOMPARE(getIndex("subchild", proxy).isValid(), true);

        QCOMPARE(spy.mSignals, QStringList() << QLatin1String("rowsInserted"));
    }

    void testInsert()
    {
        QStandardItemModel model;
        TestModel proxy;
        proxy.setSourceModel(&model);

        QStandardItem *row1 = new QStandardItem("row1");
        row1->setData(false);
        model.appendRow(row1);

        QStandardItem *child = new QStandardItem("child");
        child->setData(false);
        row1->appendRow(child);

        QStandardItem *child2 = new QStandardItem("child2");
        child2->setData(false);
        child->appendRow(child2);

        QCOMPARE(getIndex("row1", proxy).isValid(), false);
        QCOMPARE(getIndex("child", proxy).isValid(), false);
        QCOMPARE(getIndex("child2", proxy).isValid(), false);

        ModelSignalSpy spy(proxy);
        {
            QStandardItem *subchild = new QStandardItem("subchild");
            subchild->setData(true);
            child2->appendRow(subchild);
        }

        QCOMPARE(getIndex("row1", proxy).isValid(), true);
        QCOMPARE(spy.mSignals, QStringList() << QLatin1String("rowsInserted"));
        QCOMPARE(spy.parent, QModelIndex());
    }


    // We want to get child2 into the model which is a descendant of child.
    // child is already in the model from the neighbor2 branch. We must ensure dataChange is called on child, 
    // so child2 is included in the model.
    void testNeighborPath()
    {
        QStandardItemModel model;
        TestModel proxy;
        proxy.setSourceModel(&model);

        QStandardItem *row1 = new QStandardItem("row1");
        row1->setData(false);
        model.appendRow(row1);

        QStandardItem *child = new QStandardItem("child");
        child->setData(false);
        row1->appendRow(child);

        QStandardItem *child2 = new QStandardItem("child2");
        child2->setData(false);
        child->appendRow(child2);

        {
            QStandardItem *nb1 = new QStandardItem("neighbor");
            nb1->setData(false);
            child->appendRow(nb1);

            QStandardItem *nb2 = new QStandardItem("neighbor2");
            nb2->setData(true);
            nb1->appendRow(nb2);
        }

        //These tests affect the test. It seems without them the mapping is not created in qsortfilterproxymodel, resulting in the item
        //simply getting added later on. With these the model doesn't react to the added subchild as it should. Piece of crap.
        QCOMPARE(getIndex("child2", proxy).isValid(), false);
        QCOMPARE(getIndex("child", proxy).isValid(), true);
        QCOMPARE(getIndex("neighbor", proxy).isValid(), true);
        QCOMPARE(getIndex("neighbor2", proxy).isValid(), true);

        ModelSignalSpy spy(proxy);

        {
            qDebug() << "inserting";
            QStandardItem *subchild = new QStandardItem("subchild");
            subchild->setData(true);
            child2->appendRow(subchild);
        }

        QCOMPARE(getIndex("child2", proxy).isValid(), true);
        QCOMPARE(getIndex("subchild", proxy).isValid(), true);
        //The dataChanged signals are not intentional and cause by refreshAscendantMapping. Unfortunately we can't avoid them.
        QCOMPARE(spy.mSignals, QStringList() << QLatin1String("rowsInserted") << QLatin1String("dataChanged") << QLatin1String("dataChanged"));
    }

};

QTEST_KDEMAIN(KRecursiveFilterProxyModelTest, NoGUI)

#include "krecursivefilterproxymodeltest.moc"
