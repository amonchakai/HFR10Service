/*
 * HubIntegration.hpp
 *
 *  Created on: 24 mai 2014
 *      Author: pierre
 */

#ifndef HUBINTEGRATION_HPP_
#define HUBINTEGRATION_HPP_

#include <QtCore/QObject>
#include <QtCore/QSettings>


class UDSUtil;
class HubCache;

class HubIntegration : public QObject {
    Q_OBJECT;

private:
    UDSUtil     *m_UDSUtils;
    HubCache    *m_HubCache;
    QSettings   *m_Settings;
    bool         m_Initialized;
    bool         m_CategoriesInitialized;
    int          m_NbHubCreated;

    QString                                          m_ItemMimeType;
    QString                                          m_ItemUnreadIconFilename;
    QString                                          m_ItemReadIconFilename;


public:
    HubIntegration(QObject *parent = 0);
    virtual ~HubIntegration();

    inline HubCache*         getCache()          { return m_HubCache; }

    void initialize();
    void createAccount();

    void remove();
    bool addHubItem(qint64 categoryId, QVariantMap &itemMap, QString name, QString subject, qint64 timestamp, QString itemSyncId,  QString itemUserData, QString itemExtendedData, bool notify);
    void initializeCategories(QVariantList newCategories);
    QVariant*  getHubItem(qint64 categoryId, qint64 itemId);
    QVariant*  getHubItemBySyncID(qint64 categoryId, QString syncId);
    QVariantList items();
    bool updateHubItem(qint64 categoryId, qint64 itemId, QVariantMap &itemMap, bool notify);
    bool removeHubItem(qint64 categoryId, qint64 itemId);
    bool markHubItemRead(qint64 categoryId, qint64 itemId);
    bool markHubItemUnread(qint64 categoryId, qint64 itemId);
    void markHubItemsReadBefore(qint64 categoryId, qint64 timestamp);
    void removeHubItemsBefore(qint64 categoryId, qint64 timestamp);
    void repopulateHub();

};



#endif /* HUBINTEGRATION_HPP_ */
