/*
    Copyright (c) 2009 Stephen Kelly <steveire@gmail.com>

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

#include "krecursivefilterproxymodel.h"

#include <kdebug.h>

// Maintainability note:
// This class invokes some Q_PRIVATE_SLOTs in QSortFilterProxyModel which are
// private API and could be renamed or removed at any time.
// If they are renamed, the invokations can be updated with an #if (QT_VERSION(...))
// If they are removed, then layout{AboutToBe}Changed signals should be used when the source model
// gets new rows or has rowsremoved or moved. The Q_PRIVATE_SLOT invokation is an optimization
// because layout{AboutToBe}Changed is expensive and causes the entire mapping of the tree in QSFPM
// to be cleared, even if only a part of it is dirty.
// Stephen Kelly, 30 April 2010.

enum FilteringState { FilteringUnknown /* never in the map*/, FilteredIn, IndirectlyIn, Out };

QDebug operator<<(QDebug dbg, FilteringState status) {
    switch (status) {
    case FilteringUnknown:
        dbg << "Unknown";
        break;
    case FilteredIn:
        dbg << "In";
        break;
    case IndirectlyIn:
        dbg << "IndirectlyIn";
        break;
    case Out:
        dbg << "Out";
        break;
    }
    return dbg;
}

class KRecursiveFilterProxyModelPrivate
{
  Q_DECLARE_PUBLIC(KRecursiveFilterProxyModel)
  KRecursiveFilterProxyModel *q_ptr;
public:
  KRecursiveFilterProxyModelPrivate(KRecursiveFilterProxyModel *model)
    : q_ptr(model),
      ignoreRemove(false),
      completeInsert(false)
  {
    qRegisterMetaType<QModelIndex>( "QModelIndex" );
  }

  // Convenience methods for invoking the QSFPM slots. Those slots must be invoked with invokeMethod
  // because they are Q_PRIVATE_SLOTs
  inline void invokeDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
  {
    Q_Q(KRecursiveFilterProxyModel);
    bool success = QMetaObject::invokeMethod(q, "_q_sourceDataChanged", Qt::DirectConnection,
        Q_ARG(QModelIndex, topLeft),
        Q_ARG(QModelIndex, bottomRight));
    Q_UNUSED(success);
    Q_ASSERT(success);
  }

  inline void invokeRowsInserted(const QModelIndex &source_parent, int start, int end)
  {
    Q_Q(KRecursiveFilterProxyModel);
    bool success = QMetaObject::invokeMethod(q, "_q_sourceRowsInserted", Qt::DirectConnection,
        Q_ARG(QModelIndex, source_parent),
        Q_ARG(int, start),
        Q_ARG(int, end));
    Q_UNUSED(success);
    Q_ASSERT(success);
  }

  inline void invokeRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end)
  {
    Q_Q(KRecursiveFilterProxyModel);
    bool success = QMetaObject::invokeMethod(q, "_q_sourceRowsAboutToBeInserted", Qt::DirectConnection,
        Q_ARG(QModelIndex, source_parent),
        Q_ARG(int, start),
        Q_ARG(int, end));
    Q_UNUSED(success);
    Q_ASSERT(success);
  }

  inline void invokeRowsRemoved(const QModelIndex &source_parent, int start, int end)
  {
    Q_Q(KRecursiveFilterProxyModel);
    bool success = QMetaObject::invokeMethod(q, "_q_sourceRowsRemoved", Qt::DirectConnection,
        Q_ARG(QModelIndex, source_parent),
        Q_ARG(int, start),
        Q_ARG(int, end));
    Q_UNUSED(success);
    Q_ASSERT(success);
  }

  inline void invokeRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end)
  {
    Q_Q(KRecursiveFilterProxyModel);
    bool success = QMetaObject::invokeMethod(q, "_q_sourceRowsAboutToBeRemoved", Qt::DirectConnection,
        Q_ARG(QModelIndex, source_parent),
        Q_ARG(int, start),
        Q_ARG(int, end));
    Q_UNUSED(success);
    Q_ASSERT(success);
  }

  void sourceDataChanged(const QModelIndex &source_top_left, const QModelIndex &source_bottom_right);
  void sourceRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end);
  void sourceRowsInserted(const QModelIndex &source_parent, int start, int end);
  void sourceRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end);
  void sourceRowsRemoved(const QModelIndex &source_parent, int start, int end);
  void sourceLayoutChanged();
  void sourceModelReset();

  FilteringState filteringStatus(const QModelIndex &source_index);
  void refreshAfterRemoval(const QModelIndex &source_parent);

  void refreshLastFilteredOutAscendant(const QModelIndex &index);

  bool ignoreRemove;
  bool completeInsert;

  // The index is always at column 0
  QMap<QModelIndex, FilteringState> cache;
};

void KRecursiveFilterProxyModelPrivate::sourceDataChanged(const QModelIndex &source_top_left, const QModelIndex &source_bottom_right)
{
  QModelIndex source_parent = source_top_left.parent();
  Q_ASSERT(source_bottom_right.parent() == source_parent); // don't know how to handle different parents in this code...

    // Compare with cache
    for (int row = source_top_left.row(); row <= source_bottom_right.row(); ++row) {
        const QModelIndex index = source_top_left.sibling(row, 0);
        const FilteringState oldValue = cache.value(index, FilteringUnknown);
        if (oldValue == IndirectlyIn) // that didn't change
            continue;
        if (oldValue == Out || oldValue == FilteredIn)
            cache.remove(index);
        const FilteringState newValue = filteringStatus(index); // update cache
        if (oldValue != newValue) {
            //qDebug() << index.data() << "went from" << oldValue << "to" << newValue;

            if (newValue == Out) // treat this like row removal
                refreshAfterRemoval(source_parent);
            else if (newValue == FilteredIn || newValue == IndirectlyIn) { // treat this like row insertion
                refreshLastFilteredOutAscendant(source_parent);
            }
        }
    }

  // Tell the world.
  invokeDataChanged(source_top_left, source_bottom_right);
}

void KRecursiveFilterProxyModelPrivate::refreshLastFilteredOutAscendant(const QModelIndex &idx)
{
    //qDebug() << Q_FUNC_INFO << idx.data().toString();
    QModelIndex last;
    QModelIndex index = idx;
    while(index.isValid())
    {
        const FilteringState oldValue = cache.value(index, FilteringUnknown);
        if (oldValue == IndirectlyIn || oldValue == Out) {
            //qDebug() << "removed from cache" << index.data().toString();
            cache.remove(index);
        }
        if (oldValue != Out) {
            break;
        }

        last = index;
        index = index.parent();
    }

    if (last.isValid()) {
        //qDebug() << "  dataChanged" << last.data().toString();
        invokeDataChanged(last, last);
    }
}

void KRecursiveFilterProxyModelPrivate::sourceRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end)
{
  Q_Q(KRecursiveFilterProxyModel);

  if (!source_parent.isValid() || q->filterAcceptsRow(source_parent.row(), source_parent.parent()))
  {
    // If the parent is already in the model (directly or indirectly), we can just pass on the signal.
    invokeRowsAboutToBeInserted(source_parent, start, end);
    completeInsert = true;
  }
}

void KRecursiveFilterProxyModelPrivate::sourceRowsInserted(const QModelIndex &source_parent, int start, int end)
{
  Q_Q(KRecursiveFilterProxyModel);


    // Clear cache due to the row-number offset, otherwise we'll use wrongly cached data
    QAbstractItemModel *sourceModel = q->sourceModel();
    for (int row = start ; row < sourceModel->rowCount(source_parent) ; ++row) {
        const QModelIndex index = sourceModel->index(row, 0, source_parent);
        //qDebug() << "Clearing cache for" << index.data().toString();
        cache.remove(index);
    }

  if (completeInsert)
  {
    // If the parent is already in the model, we can just pass on the signal.
    completeInsert = false;
    invokeRowsInserted(source_parent, start, end);
    return;
  }


  bool requireRow = false;
  for (int row = start; row <= end; ++row)
  {
    if (q->filterAcceptsRow(row, source_parent))
    {
      requireRow = true;
      break;
    }
  }

  if (!requireRow)
  {
    // The new rows doesn't have any descendants that match the filter. Filter them out.
    return;
  }

  //qDebug() << "refreshLastFilteredOutAscendant" << source_parent.data().toString();
    refreshLastFilteredOutAscendant(source_parent);
}

void KRecursiveFilterProxyModelPrivate::refreshAfterRemoval(const QModelIndex &source_parent)
{
    Q_Q(KRecursiveFilterProxyModel);
    // Find out if removing this visible row means that some ascendant
    // row can now be hidden.
    // We go up until we find a row that should still be visible
    // and then make QSFPM re-evaluate the last one we saw before that, to hide it.

    QModelIndex toHide;
    QModelIndex index = source_parent;
    while(index.isValid())
    {
        const FilteringState oldValue = cache.value(index, FilteringUnknown);
        //qDebug() << "after removal:" << index.data().toString() << "was" << oldValue;
        if (oldValue == IndirectlyIn || oldValue == FilteredIn) {
            cache.remove(index);
            if (q->filterAcceptsRow(index.row(), index.parent())) {
                //qDebug() << "Now it's in";
                break;
            }
        }
        toHide = index;
        index = index.parent();
    }
    if (toHide.isValid()) {
        //qDebug() << "hiding" << toHide.data().toString();
        invokeDataChanged(toHide, toHide);
    }
}

void KRecursiveFilterProxyModelPrivate::sourceRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end)
{
  Q_Q(KRecursiveFilterProxyModel);

  bool accepted = false;
  for (int row = start; row <= end; ++row)
  {
    if (q->filterAcceptsRow(row, source_parent))
    {
      accepted = true;
      break;
    }
  }
  if (!accepted)
  {
    // All removed rows are already filtered out. We don't care about the signal.
    ignoreRemove = true;
    return;
  }

  invokeRowsAboutToBeRemoved(source_parent, start, end);
}

void KRecursiveFilterProxyModelPrivate::sourceRowsRemoved(const QModelIndex &source_parent, int start, int end)
{
  if (ignoreRemove)
  {
    ignoreRemove = false;
    return;
  }

  Q_Q(KRecursiveFilterProxyModel);

  // Clear cache due to the row-number offset, otherwise we'll use wrongly cached data
  QAbstractItemModel *sourceModel = q->sourceModel();
  for (int row = start ; row < sourceModel->rowCount(source_parent) ; ++row) {
      const QModelIndex index = sourceModel->index(row, 0, source_parent);
      cache.remove(index);
  }

  // TODO remove from the cache all the children of the removed rows....
  //qDebug() << "sourceRowsRemoved" << source_parent.data() << start << end;
  invokeRowsRemoved(source_parent, start, end);
  refreshAfterRemoval(source_parent);
}

void KRecursiveFilterProxyModelPrivate::sourceLayoutChanged()
{
    cache.clear();
}

void KRecursiveFilterProxyModelPrivate::sourceModelReset()
{
    cache.clear();
}

FilteringState KRecursiveFilterProxyModelPrivate::filteringStatus(const QModelIndex &source_index)
{
    Q_Q(KRecursiveFilterProxyModel);

    // Do not use the cache here, only write to it.
    // This is because the filtering status of indexes can change without us being informed about it,
    // e.g. due to setFilterFixedString().

    if (q->acceptRow(source_index.row(), source_index.parent())) {
        //qDebug() << "    writing cache (In) :" << source_index.data().toString() << FilteredIn;
        cache.insert(source_index, FilteredIn);
        return FilteredIn;
    }

    bool accepted = false;
    for (int row = 0 ; row < source_index.model()->rowCount(source_index); ++row) {
        if (q->filterAcceptsRow(row, source_index)) {
            accepted = true;
            break;
        }
    }

    const FilteringState status = accepted ? IndirectlyIn : Out;
    //qDebug() << "    writing cache:" << source_index.data().toString() << status;
    cache.insert(source_index, status);
    return status;
}

KRecursiveFilterProxyModel::KRecursiveFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent), d_ptr(new KRecursiveFilterProxyModelPrivate(this))
{
  setDynamicSortFilter(true);
}

KRecursiveFilterProxyModel::~KRecursiveFilterProxyModel()
{
  delete d_ptr;
}

bool KRecursiveFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex source_index = sourceModel()->index(sourceRow, 0, sourceParent);
    Q_ASSERT(source_index.isValid());

    return d_ptr->filteringStatus(source_index) != Out;
}

QModelIndexList KRecursiveFilterProxyModel::match( const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags ) const
{
  if ( role < Qt::UserRole )
    return QSortFilterProxyModel::match( start, role, value, hits, flags );

  QModelIndexList list;
  QModelIndex proxyIndex;
  foreach ( const QModelIndex &idx, sourceModel()->match( mapToSource( start ), role, value, hits, flags ) ) {
    proxyIndex = mapFromSource( idx );
    if ( proxyIndex.isValid() )
      list << proxyIndex;
  }

  return list;
}

bool KRecursiveFilterProxyModel::acceptRow(int sourceRow, const QModelIndex& sourceParent) const
{
  return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

void KRecursiveFilterProxyModel::setSourceModel(QAbstractItemModel* model)
{
  // Standard disconnect.
  disconnect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
      this, SLOT(sourceDataChanged(QModelIndex,QModelIndex)));

  disconnect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
      this, SLOT(sourceRowsAboutToBeInserted(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
      this, SLOT(sourceRowsInserted(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
      this, SLOT(sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
      this, SLOT(sourceRowsRemoved(QModelIndex,int,int)));

  disconnect(model, SIGNAL(layoutChanged()),
      this, SLOT(sourceLayoutChanged()));

  disconnect(model, SIGNAL(modelReset()),
      this, SLOT(sourceModelReset()));

  QSortFilterProxyModel::setSourceModel(model);

  // Disconnect in the QSortFilterProxyModel. These methods will be invoked manually
  // in invokeDataChanged, invokeRowsInserted etc.
  //
  // The reason for that is that when the source model adds new rows for example, the new rows
  // May not match the filter, but maybe their child items do match.
  //
  // Source model before insert:
  //
  // - A
  // - B
  // - - C
  // - - D
  // - - - E
  // - - - F
  // - - - G
  // - H
  // - I
  //
  // If the A F and L (which doesn't exist in the source model yet) match the filter
  // the proxy will be:
  //
  // - A
  // - B
  // - - D
  // - - - F
  //
  // New rows are inserted in the source model below H:
  //
  // - A
  // - B
  // - - C
  // - - D
  // - - - E
  // - - - F
  // - - - G
  // - H
  // - - J
  // - - K
  // - - - L
  // - I
  //
  // As L matches the filter, it should be part of the KRecursiveFilterProxyModel.
  //
  // - A
  // - B
  // - - D
  // - - - F
  // - H
  // - - K
  // - - - L
  //
  // when the QSortFilterProxyModel gets a notification about new rows in H, it only checks
  // J and K to see if they match, ignoring L, and therefore not adding it to the proxy.
  // To work around that, we make sure that the QSFPM slot which handles that change in
  // the source model (_q_sourceRowsAboutToBeInserted) does not get called directly.
  // Instead we connect the sourceModel signal to our own slot in *this (sourceRowsAboutToBeInserted)
  // Inside that method, the entire new subtree is queried (J, K *and* L) to see if there is a match,
  // then the relevant slots in QSFPM are invoked.
  // In the example above, we need to tell the QSFPM that H should be queried again to see if
  // it matches the filter. It did not before, because L did not exist before. Now it does. That is
  // achieved by telling the QSFPM that the data changed for H, which causes it to requery this class
  // to see if H matches the filter (which it now does as L now exists).
  // That is done in sourceRowsInserted.

  disconnect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
      this, SLOT(_q_sourceDataChanged(QModelIndex,QModelIndex)));

  disconnect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
      this, SLOT(_q_sourceRowsAboutToBeInserted(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
      this, SLOT(_q_sourceRowsInserted(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
      this, SLOT(_q_sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

  disconnect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
      this, SLOT(_q_sourceRowsRemoved(QModelIndex,int,int)));

  // Slots for manual invoking of QSortFilterProxyModel methods.
  connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
      this, SLOT(sourceDataChanged(QModelIndex,QModelIndex)));

  connect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
      this, SLOT(sourceRowsAboutToBeInserted(QModelIndex,int,int)));

  connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
      this, SLOT(sourceRowsInserted(QModelIndex,int,int)));

  connect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
      this, SLOT(sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

  connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
      this, SLOT(sourceRowsRemoved(QModelIndex,int,int)));

  connect(model, SIGNAL(layoutChanged()),
      this, SLOT(sourceLayoutChanged()));

  connect(model, SIGNAL(modelReset()),
      this, SLOT(sourceModelReset()));
}

#include "krecursivefilterproxymodel.moc"
