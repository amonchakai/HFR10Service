/*
 * HubIntegration.cpp
 *
 *  Created on: 24 mai 2014
 *      Author: pierre
 */

#include "HubIntegration.hpp"
#include "UDSUtil.hpp"
#include "HubCache.hpp"
#include <QDebug>

HubIntegration::HubIntegration(QObject *parent) :
            QObject(parent),
            m_UDSUtils(NULL),
            m_HubCache(NULL),
            m_Settings(NULL),
            m_Initialized(false),
            m_CategoriesInitialized(false),
            m_ItemMimeType("hub/vnd.hfrblack.item"),
            m_ItemUnreadIconFilename("images/icon_MarkUnread.png"),
            m_ItemReadIconFilename("images/icon_MarkRead.png") {

    m_Settings = new QSettings("amonchakai.dev", "HFR10");
    m_UDSUtils = new UDSUtil("com.HFR10.HubIntegration", QString("hubassets/"));
    m_UDSUtils->initialize();
    m_HubCache = new HubCache(m_Settings);

    // on device restart / update, it may be necessary to reload the Hub
    if (m_UDSUtils->reloadHub()) {
        m_UDSUtils->cleanupAccountsExcept(-1, "HFR10");
        m_UDSUtils->initNextIds();
    }

    initialize();
    //remove();

}


HubIntegration::~HubIntegration() {

}


void HubIntegration::initialize() {
    if(m_Initialized)
        return ;


    if (m_HubCache->accountId() <= 0) {

        qint64 hubAccountId = m_UDSUtils->addAccount("HFR10", "HFR10", "com.HFR10.hub", "pierre.lebreton.HFRBlack",
                                    "images/whiteFace.png", "", "images/whiteFace.png", QString(tr("Private message")),
                                    true, UDS_ACCOUNT_TYPE_SOCIAL);

        if(hubAccountId > 0) {
            m_HubCache->setAccountId(hubAccountId);
            m_HubCache->setAccountName("HFR10");
        } else {
            qWarning() << "Registration to hub failed!";
        }

        int retVal = m_UDSUtils->addAccountAction(m_HubCache->accountId(), QString("bb.action.COMPOSE"), QString(tr("Compose")),
                                                     "pierre.lebreton.HFRBlack", QString("application"), "images/icon_write.png", m_ItemMimeType, UDS_PLACEMENT_BAR);
        if (retVal != 0) {
            qDebug() << "HubAccount::initialize: addAccountActionData: bb.action.COMPOSE : " << " retval: " << retVal;
        }

        retVal = m_UDSUtils->addItemAction(m_HubCache->accountId(), QString("bb.action.MARKREAD"), QString(tr("Mark Read")),
                                         "pierre.lebreton.HFRBlack.Headless", QString("application.headless"), m_ItemReadIconFilename, m_ItemMimeType, UDS_PLACEMENT_OVERFLOW);
        if (retVal != 0) {
            qDebug() << "HubAccount::addHubItem: addItmActionData: addItmAction: bb.action.MARKREAD : " << " retval: " << retVal;
        }

        retVal = m_UDSUtils->addItemAction(m_HubCache->accountId(), QString("bb.action.MARKUNREAD"), QString(tr("Mark Unread")),
                                        "pierre.lebreton.HFRBlack.Headless", QString("application.headless"), m_ItemUnreadIconFilename, m_ItemMimeType, UDS_PLACEMENT_OVERFLOW);
        if (retVal != 0) {
            qDebug() << "HubAccount::addHubItem: addItmActionData: addItmAction: bb.action.MARKUNREAD : " << " retval: " << retVal;
        }

    }  else {
         m_UDSUtils->restoreNextIds(m_HubCache->accountId(), m_HubCache->lastCategoryId()+1, m_HubCache->lastItemId()+1);
        // remove();
    }



    m_Initialized = true;

}


void HubIntegration::remove() {

    if(!m_UDSUtils && !m_HubCache)
        return;

    m_Initialized = false;

    // m_HubCache->setAccountId(-1); // does not work: settings only records >0
    if(m_UDSUtils->removeAccount(m_HubCache->accountId())) {
        m_UDSUtils->cleanupAccountsExcept(m_HubCache->accountId(), m_HubCache->accountName());
    }
}


bool HubIntegration::addHubItem(qint64 categoryId, QVariantMap &itemMap, QString name, QString subject, qint64 timestamp, QString itemSyncId,  QString itemUserData, QString itemExtendedData, bool notify) {
    qint64 retVal = 0;

    bool    itemRead = false;
    int itemContextState = 1;

    qDebug() << "add hub item: " << timestamp << " - " << name << " - " << subject;

    retVal = m_UDSUtils->addItem(m_HubCache->accountId(), categoryId, itemMap, name, subject, m_ItemMimeType, m_ItemUnreadIconFilename,
                                itemRead, itemSyncId, itemUserData, itemExtendedData, timestamp, itemContextState, notify);

    if (retVal <= 0) {
        qDebug() << "HubAccount::addHubItem: addItem failed for item: " << name << ", category: " << categoryId << ", account: "<< m_HubCache->accountId() << ", retVal: "<< retVal << "\n";
    } else {
        m_HubCache->addItem(itemMap);
    }

    return (retVal > 0);
}



void HubIntegration::initializeCategories(QVariantList newCategories) {
    qDebug()  << "HubAccount::initializeCategories " << m_CategoriesInitialized;

    if (!m_CategoriesInitialized) {
        qint64 retVal = -1;

        if (m_HubCache->categories().size() == 0) {
            QVariantList categories;

            for(int index = 0; index < newCategories.size(); index++) {
                QVariantMap category = newCategories[index].toMap();
                retVal = m_UDSUtils->addCategory(m_HubCache->accountId(), category["name"].toString(), category["parentCategoryId"].toLongLong());
                if (retVal == -1) {
                    qDebug() << "HubAccount::initializeCategories: add category failed for: " << category;
                    break;
                }

                categories << category;
            }

            if (retVal > 0) {
                m_HubCache->setCategories(categories);
            }
        }

        QVariantList items = m_HubCache->items();

        m_CategoriesInitialized = true;
    }
}


QVariant* HubIntegration::getHubItem(qint64 categoryId, qint64 itemId)
{
    QVariant* item = m_HubCache->getItem(categoryId,itemId);
    if (item) {
        QVariantMap itemMap = (*item).toMap();
    }

    return item;
}

QVariant* HubIntegration::getHubItemBySyncID(qint64 categoryId, QString syncId)
{
    QVariant* item = m_HubCache->getItemBySyncID(categoryId,syncId);
    if (item) {
        QVariantMap itemMap = (*item).toMap();
    }

    return item;
}

QVariantList HubIntegration::items()
{
    QVariantList _items = m_HubCache->items();
    QVariantList items;

    if (_items.size() > 0) {
        for(int index = 0; index < _items.size(); index++) {
            QVariantMap itemMap = _items.at(index).toMap();

            if (itemMap["accountId"].toLongLong() ==  m_HubCache->accountId()) {
                items << itemMap;
            }
        }
    }

    return items;
}


bool HubIntegration::updateHubItem(qint64 categoryId, qint64 itemId, QVariantMap &itemMap, bool notify)
{
    qint64 retVal = 0;
    int itemContextState = 1;

    qDebug() << "update hub item: " << categoryId << " : " <<  itemId << " : " <<  itemMap << " : " <<  notify;

    if ((itemMap["readCount"].toInt() > 0)) {
        retVal = m_UDSUtils->updateItem(m_HubCache->accountId(), categoryId, itemMap, QString::number(itemId), itemMap["name"].toString(), itemMap["description"].toString(), m_ItemMimeType, m_ItemReadIconFilename,
                (itemMap["readCount"].toInt() > 0), itemMap["syncId"].toString(), itemMap["userData"].toString(), itemMap["extendedData"].toString(), itemMap["timestamp"].toLongLong(), itemContextState, notify);
    } else {
        retVal = m_UDSUtils->updateItem(m_HubCache->accountId(), categoryId, itemMap, QString::number(itemId), itemMap["name"].toString(), itemMap["description"].toString(), m_ItemMimeType, m_ItemUnreadIconFilename,
                (itemMap["readCount"].toInt() > 0), itemMap["syncId"].toString(), itemMap["userData"].toString(), itemMap["extendedData"].toString(), itemMap["timestamp"].toLongLong(), itemContextState, notify);
    }

    if (retVal <= 0) {
        qDebug() << "HubAccount::updateHubItem: updateItem failed for item: " << itemId << ", category: " << categoryId << ", account: "<< m_HubCache->accountId() << ", retVal: "<< retVal << "\n";
    } else {
        m_HubCache->updateItem(itemId, itemMap);
    }

    return (retVal > 0);
}

bool HubIntegration::removeHubItem(qint64 categoryId, qint64 itemId)
{
    qint64 retVal = 0;

    qDebug() << "remove hub item: " << categoryId << " : " <<  itemId;

    retVal = m_UDSUtils->removeItem(m_HubCache->accountId(), categoryId, QString::number(itemId));
    if (retVal <= 0) {
        qDebug() << "HubAccount::removeHubItem: removeItem failed for item: " << categoryId << " : " <<  itemId << ", retVal: "<< retVal << "\n";
    } else {
        m_HubCache->removeItem(itemId);
    }

    return (retVal > 0);
}

bool HubIntegration::markHubItemRead(qint64 categoryId, qint64 itemId)
{
    bool retVal = false;
    qDebug()  << "HubAccount::markHubItemRead: " << categoryId << " : " << itemId;

    QVariant* item = getHubItem(categoryId, itemId);

    if (item) {
        QVariantMap itemMap = item->toMap();

        itemMap["readCount"] = 1;
        itemMap["totalCount"] = 1;

        qDebug()  << "HubAccount::markHubItemRead: itemMap: " << itemMap;

        retVal = updateHubItem(categoryId, itemId, itemMap, false);
    }

    return retVal;
}

bool HubIntegration::markHubItemUnread(qint64 categoryId, qint64 itemId)
{
    bool retVal = false;
    qDebug()  << "HubAccount::markHubItemUnread: " << categoryId << " : " << itemId;

    QVariant* item = getHubItem(categoryId, itemId);

    if (item) {
        QVariantMap itemMap = item->toMap();

        itemMap["readCount"] = 0;
        itemMap["totalCount"] = 1;

        qDebug()  << "HubAccount::markHubItemUnread: itemMap: " << itemMap;

        retVal = updateHubItem(categoryId, itemId, itemMap, false);
    }

    return retVal;
}

void HubIntegration::markHubItemsReadBefore(qint64 categoryId, qint64 timestamp)
{
    qDebug()  << "HubAccount::markHubItemsReadBefore: " << categoryId << " : " << timestamp;

    QVariantList _items = items();
    QVariantMap itemMap;
    qint64 itemId;
    qint64 itemCategoryId;
    qint64 itemTime;

    for(int index = 0; index < _items.size(); index++) {
        itemMap = _items.at(index).toMap();
        itemId = itemMap["sourceId"].toLongLong();
        itemCategoryId = itemMap["categoryId"].toLongLong();
        itemTime = itemMap["timestamp"].toLongLong();

        qDebug()  << "HubAccount::markHubItemUnread: checking itemMap: " << itemId << " : " << itemCategoryId << " : " << itemTime << " : " << itemMap;

        if (itemTime < timestamp && itemCategoryId == categoryId) {
            itemMap["readCount"] = 1;
            updateHubItem(itemCategoryId, itemId, itemMap, false);
            qDebug()  << "HubAccount::markHubItemUnread: marking as read: ";
        }
    }
}

void HubIntegration::removeHubItemsBefore(qint64 categoryId, qint64 timestamp)
{
    bool retVal = false;

    qDebug()  << "HubAccount::removeHubItemsBefore: " << categoryId << " : " << timestamp;

    QVariantList _items;
    QVariantMap itemMap;
    qint64 itemId;
    qint64 itemCategoryId;
    qint64 itemTime;

    bool foundItems = false;
    do {
        foundItems = false;

        _items = items();
        for(int index = 0; index < _items.size(); index++) {
            itemMap = _items.at(index).toMap();
            itemId = itemMap["sourceId"].toLongLong();
            itemCategoryId = itemMap["categoryId"].toLongLong();
            itemTime = itemMap["timestamp"].toLongLong();

            qDebug()  << "HubAccount::removeHubItemsBefore: checking itemMap: " << itemId << " : " << itemCategoryId << " : " << itemTime << " : " << itemMap;

            if (itemTime < timestamp && itemCategoryId == categoryId) {
                retVal = removeHubItem(itemCategoryId, itemId);
                if (retVal) {
                    foundItems = true;
                    qDebug()  << "HubAccount::removeHubItemsBefore: deleted: ";
                }
                break;
            }
        }
    } while (foundItems);
}

void HubIntegration::repopulateHub()
{
    QVariantList items = m_HubCache->items();

    if (items.size() > 0) {
        for(int index = 0; index < items.size(); index++) {
            QVariantMap itemMap = items.at(index).toMap();

            if (itemMap["accountId"].toLongLong() ==  m_HubCache->accountId()) {
                addHubItem(itemMap["categoryId"].toLongLong(), itemMap, itemMap["name"].toString(), itemMap["description"].toString(), itemMap["timestamp"].toLongLong(), itemMap["syncId"].toString(), itemMap["userData"].toString(), itemMap["extendedData"].toString(), false);
            }
        }
    }
}
