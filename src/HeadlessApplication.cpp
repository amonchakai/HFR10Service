/*
 * Copyright (c) 2011-2014 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HeadlessApplication.hpp"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>
#include <bb/data/JsonDataAccess>

#include <QTimer>
#include <QDir>
#include <QRegExp>


#include "DataObjects.h"
#include "Network/CookieJar.hpp"
#include "Hub/HubIntegration.hpp"
#include "Hub/HubCache.hpp"

using namespace bb::cascades;

HeadlessApplication::HeadlessApplication(bb::Application *app) :
        QObject(app),
        m_InvokeManager(new bb::system::InvokeManager()),
        m_app(app),
        m_ListFavoriteController(NULL),
        m_AppSettings(NULL) {

    m_InvokeManager->setParent(this);

    // prepare the localization
    //m_pTranslator = new QTranslator(this);
    //m_pLocaleHandler = new LocaleHandler(this);

    // ---------------------------------------------------------------------
    // HUB integration
    m_Hub = new HubIntegration();

    QVariantList categories;
    QVariantMap map;
    map["name"] = "Private Message";
    categories.push_back(map);

    m_Hub->initializeCategories(categories);

    m_Hub->removeHubItemsBefore(m_Hub->getCache()->lastCategoryId(), QDateTime::currentDateTime().addYears(1).toMSecsSinceEpoch());


    m_AppSettings = new Settings();
    m_AppSettings->loadSettings();

    // ---------------------------------------------------------------------
    // prepare to process events

    qDebug() << "-----------------------------------\nStart Headless app!...\nWait for ticks\n------------------------------------";


    m_Timer = new QTimer(this);
    m_Timer->setInterval(m_AppSettings->getHubRefreshRate() * 60000);         // refresh based on the setting (in minutes)
//    m_Timer->setInterval(10000);         // every 10s
    m_Timer->setSingleShot(false);
    connect(m_Timer, SIGNAL(timeout()), this, SLOT(periodicCheck()));
    m_Timer->start();

    CookieJar *cookies = CookieJar::get();
    cookies->loadFromDisk();



    m_PrivateMessageController = new PrivateMessageController();
    m_PrivateMessageController->setRenderLocation(IN_HUB);

    m_ListFavoriteController = new ListFavoriteController();
    m_ListFavoriteController->setRenderLocation(IN_HUB);

    bool connectResult;

    Q_UNUSED(connectResult);
    connectResult = connect( m_InvokeManager,
             SIGNAL(invoked(const bb::system::InvokeRequest&)),
             this,
             SLOT(onInvoked(const bb::system::InvokeRequest&)));

    Q_ASSERT(connectResult);

    Q_UNUSED(connectResult);
    connectResult = connect( m_PrivateMessageController,
             SIGNAL(complete()),
             this,
             SLOT(insertNewEntries()));

    Q_ASSERT(connectResult);

    Q_UNUSED(connectResult);
    connectResult = connect( m_PrivateMessageController,
                 SIGNAL(autentificationFailed()),
                 this,
                 SLOT(checkLogin()));

    Q_ASSERT(connectResult);

    Q_UNUSED(connectResult);
    connectResult = connect( m_ListFavoriteController,
                SIGNAL(complete()),
                this,
                SLOT(insertNewFavEntries()));

    Q_ASSERT(connectResult);
/*
    bool res = QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()), this, SLOT(onSystemLanguageChanged()));
    // This is only available in Debug builds
    Q_ASSERT(res);
    Q_UNUSED(res);

    // initial load
    onSystemLanguageChanged();
*/

}


void HeadlessApplication::onSystemLanguageChanged()
{
    QCoreApplication::instance()->removeTranslator(m_pTranslator);
    // Initiate, load and install the application translation files.
    QString locale_string = QLocale().name();
    QString file_name = QString("HeadlessHFR_%1").arg(locale_string);
    if (m_pTranslator->load(file_name, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(m_pTranslator);
    }
}


void HeadlessApplication::checkLogin() {
    CookieJar *cookies = CookieJar::get();
    cookies->loadFromDisk();
}

void HeadlessApplication::onInvoked(const bb::system::InvokeRequest& request) {
    qDebug() << "invoke Headless!" << request.action();

    if(request.action().compare("bb.action.system.STARTED") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : auto started";
        } else if(request.action().compare("bb.action.START") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : start";
        } else if(request.action().compare("bb.action.STOP") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : stop";
            //_app->quit();
            m_app->requestExit();

        } else if(request.action().compare("bb.action.MARKREAD") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: mark read" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            markHubItemRead(attributesMap);

        } else if(request.action().compare("bb.action.MARKUNREAD") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: mark unread" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            markHubItemUnread(attributesMap);

        } else if(request.action().compare("bb.action.MARKPRIORREAD") == 0) {
            bb::data::JsonDataAccess jda;

            qint64 timestamp = (jda.loadFromBuffer(request.data())).toLongLong();
            QDateTime date = QDateTime::fromMSecsSinceEpoch(timestamp);

            qDebug() << "HeadlessHubIntegration: onInvoked: mark prior read : " << timestamp << " : " << request.data();

            m_Hub->markHubItemsReadBefore(m_Hub->getCache()->lastCategoryId(), timestamp);

        } else if(request.action().compare("bb.action.DELETE") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : delete" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            removeHubItem(attributesMap);

        } else if(request.action().compare("bb.action.DELETEPRIOR") == 0) {
            bb::data::JsonDataAccess jda;

            qint64 timestamp = (jda.loadFromBuffer(request.data())).toLongLong();
            QDateTime date = QDateTime::fromMSecsSinceEpoch(timestamp);

            qDebug() << "HeadlessHubIntegration: onInvoked: mark prior delete : " << timestamp << " : " << request.data();

            m_Hub->removeHubItemsBefore(m_Hub->getCache()->lastCategoryId(), timestamp);

        } else {
            qDebug() << "HeadlessHubIntegration: onInvoked: unknown service request " << request.action() << " : " << request.data();
        }

}



void HeadlessApplication::markHubItemRead(QVariantMap itemProperties) {

    qint64 itemId;

    if (itemProperties["sourceId"].toString().length() > 0) {
        itemId = itemProperties["sourceId"].toLongLong();
    } else if (itemProperties["messageid"].toString().length() > 0) {
        itemId = itemProperties["messageid"].toLongLong();
    }

    m_Hub->markHubItemRead(m_Hub->getCache()->lastCategoryId(), itemId);
}

void HeadlessApplication::markHubItemUnread(QVariantMap itemProperties) {

    qint64 itemId;

    if (itemProperties["sourceId"].toString().length() > 0) {
        itemId = itemProperties["sourceId"].toLongLong();
    } else if (itemProperties["messageid"].toString().length() > 0) {
        itemId = itemProperties["messageid"].toLongLong();
    }

    m_Hub->markHubItemUnread(m_Hub->getCache()->lastCategoryId(), itemId);
}

void HeadlessApplication::removeHubItem(QVariantMap itemProperties) {

    qint64 itemId;
    if (itemProperties["sourceId"].toString().length() > 0) {
        itemId = itemProperties["sourceId"].toLongLong();
    } else if (itemProperties["messageid"].toString().length() > 0) {
        itemId = itemProperties["messageid"].toLongLong();
    }

    m_Hub->removeHubItem(m_Hub->getCache()->lastCategoryId(), itemId);
}


void HeadlessApplication::periodicCheck() {

    //qDebug() << "tick!";

    m_AppSettings->loadSettings();
    m_Timer->setInterval(m_AppSettings->getHubRefreshRate() * 60000);

    m_PrivateMessageController->getMessages();
    if(m_AppSettings->getNotifBlue() || m_AppSettings->getNotifGreen() || m_AppSettings->getNotifOrange() || m_AppSettings->getNotifPink() || m_AppSettings->getNotifPurple()) {
        m_ListFavoriteController->getFavorite();
    }

}

void HeadlessApplication::insertNewEntries() {
    m_LockHubItem.lockForWrite();

    const QList<PrivateMessageListItem*>    *list     = m_PrivateMessageController->getMessageList();

    for(int i = 0 ; i < list->length() ; ++i) {

        QString threadID;
        QRegExp  firstPageShortUrl("sujet_([0-9]+)_1.htm");
        if(firstPageShortUrl.indexIn(list->at(i)->getUrlFirstPage()) != -1) {
            threadID = firstPageShortUrl.cap(1);
        } else {
            QRegExp firstPageDetailedUrl("post=([0-9]+)");
            if(firstPageDetailedUrl.indexIn(list->at(i)->getUrlFirstPage()) != -1) {
                threadID = firstPageDetailedUrl.cap(1);
            }
        }

        //qDebug() << "MP" << i << "ThreadID: " << threadID;


        bool inHub = false;
        int n;
        for(n = 0 ; n < m_ItemsInHub.length() ; ++n) {

            if(m_ItemsInHub.at(n).m_ThreadId == threadID) {
                inHub = true;
                break;
            }
        }



        if(!inHub) {
            QVariantMap entry;
            entry["url"] = list->at(i)->getUrlLastPage();

            //            QDateTime::fromString(s.mid(0,10) + s.mid(12), "dd-MM-yyyy HH:mm");
            m_Hub->addHubItem(m_Hub->getCache()->lastCategoryId(), entry, list->at(i)->getAddressee(), list->at(i)->getTitle(), QDateTime::fromString(list->at(i)->getTimestamp().mid(0,10) + list->at(i)->getTimestamp().mid(12), "dd-MM-yyyy HH:mm").toMSecsSinceEpoch(), QString::number(1),"", "",  list->at(i)->isRead());

            qint64 itemId;
            if (entry["sourceId"].toString().length() > 0) {
                itemId = entry["sourceId"].toLongLong();
            } else if (entry["messageid"].toString().length() > 0) {
                itemId = entry["messageid"].toLongLong();
            }

            m_ItemsInHub.push_back(HubItem());
            m_ItemsInHub.last().m_HubId = itemId;
            m_ItemsInHub.last().m_ThreadId = threadID;
            m_ItemsInHub.last().m_Timestamp = list->at(i)->getTimestamp();

            qDebug() << "entry assigned: hubid: " << m_ItemsInHub.last().m_HubId << " ThreadID " << m_ItemsInHub.last().m_ThreadId << m_ItemsInHub.last().m_Timestamp;

            if(!list->at(i)->isRead()) {
                m_Hub->markHubItemRead(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
            }

        }

        else {

            if(m_ItemsInHub.at(n).m_Timestamp != list->at(i)->getTimestamp()) {
                QVariant* item = m_Hub->getHubItem(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
                if (item) {
                    QVariantMap itemMap = item->toMap();
                    m_ItemsInHub[n].m_Timestamp = list->at(i)->getTimestamp();
                    itemMap["timestamp"] = QDateTime::fromString(list->at(i)->getTimestamp().mid(0,10) + list->at(i)->getTimestamp().mid(12), "dd-MM-yyyy HH:mm").toMSecsSinceEpoch();
                    itemMap["url"] = list->at(i)->getUrlLastPage();
                    m_Hub->updateHubItem(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId, itemMap, !list->at(i)->isRead());
                }

                if(!list->at(i)->isRead()) {
                    m_Hub->markHubItemRead(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
                } else {
                    m_Hub->markHubItemUnread(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
                }
            }

        }


    }

    m_LockHubItem.unlock();

}


void HeadlessApplication::insertNewFavEntries() {
    m_LockHubItem.lockForWrite();

    const QList<ThreadListItem*>    *list     = m_ListFavoriteController->getFavoriteList();

    int colorNotif[6];
    colorNotif[0] = 0;
    colorNotif[1] = m_AppSettings->getNotifGreen();
    colorNotif[2] = m_AppSettings->getNotifBlue();
    colorNotif[3] = m_AppSettings->getNotifOrange();
    colorNotif[4] = m_AppSettings->getNotifPink();
    colorNotif[5] = m_AppSettings->getNotifPurple();


    {   // Hub cleanup
        QRegExp isTopic("[0-9]+^[0-9][0-9]+");
        for(int i = 0 ; i < m_ItemsInHub.length() ; ++i) {
            if(isTopic.indexIn(m_ItemsInHub[i].m_ThreadId) == -1)      // check if it is a private message
                continue;

            if(!colorNotif[Settings::getTagValue(m_ItemsInHub[i].m_ThreadId)]) {
                m_Hub->removeHubItem(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub[i].m_HubId);
                m_ItemsInHub.removeAt(i);
                --i;
            }

        }
    }


    for(int i = 0 ; i < list->length() ; ++i) {

        QString threadID;
        QRegExp firstPageDetailedUrl("post=([0-9]+)");
        if(firstPageDetailedUrl.indexIn(list->at(i)->getUrlLastPostRead()) != -1) {
            threadID = firstPageDetailedUrl.cap(1);
        }

        QRegExp firstPageDetailedUrlCat("cat=([0-9]+)");
        if(firstPageDetailedUrlCat.indexIn(list->at(i)->getUrlLastPostRead()) != -1) {
            threadID += "@" + firstPageDetailedUrlCat.cap(1);
        }

        if(colorNotif[Settings::getTagValue(threadID)] == 0)
            continue;


        bool inHub = false;
        int n;
        for(n = 0 ; n < m_ItemsInHub.length() ; ++n) {

            if(m_ItemsInHub.at(n).m_ThreadId == threadID) {
                inHub = true;
                break;
            }
        }

        if(!inHub) {
            QVariantMap entry;
            entry["url"] = list->at(i)->getUrlLastPage();

                //            QDateTime::fromString(s.mid(0,10) + s.mid(12), "dd-MM-yyyy HH:mm");

            m_Hub->addHubItem(m_Hub->getCache()->lastCategoryId(), entry, list->at(i)->getTitle(), list->at(i)->getLastAuthor(), QDateTime::fromString(list->at(i)->getDetailedTimestamp().mid(0,10) + " " + list->at(i)->getDetailedTimestamp().mid(23), "dd-MM-yyyy HH:mm").toMSecsSinceEpoch(), QString::number(1),"", "",  true);

            qint64 itemId;
            if (entry["sourceId"].toString().length() > 0) {
                itemId = entry["sourceId"].toLongLong();
            } else if (entry["messageid"].toString().length() > 0) {
                itemId = entry["messageid"].toLongLong();
            }

            m_ItemsInHub.push_back(HubItem());
            m_ItemsInHub.last().m_HubId = itemId;
            m_ItemsInHub.last().m_ThreadId = threadID;
            m_ItemsInHub.last().m_Timestamp = list->at(i)->getTimestamp();

            qDebug() << "entry assigned: hubid: " << m_ItemsInHub.last().m_HubId << " ThreadID " << m_ItemsInHub.last().m_ThreadId << m_ItemsInHub.last().m_Timestamp;

        } else {

            if(m_ItemsInHub.at(n).m_Timestamp != list->at(i)->getTimestamp()) {

                QVariant* item = m_Hub->getHubItem(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
                if (item) {
                    QVariantMap itemMap = item->toMap();
                    m_ItemsInHub[n].m_Timestamp = list->at(i)->getDetailedTimestamp();
                    itemMap["timestamp"] = QDateTime::fromString(list->at(i)->getDetailedTimestamp().mid(0,10) + " " + list->at(i)->getDetailedTimestamp().mid(23), "dd-MM-yyyy HH:mm").toMSecsSinceEpoch();
                    itemMap["description"] = list->at(i)->getLastAuthor();
                    itemMap["url"] = list->at(i)->getUrlLastPostRead();
                    m_Hub->updateHubItem(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId, itemMap, false);

                    if(itemMap["readCount"] == 1)
                        m_Hub->markHubItemUnread(m_Hub->getCache()->lastCategoryId(), m_ItemsInHub.at(n).m_HubId);
                }
            }

        }


   }

   m_LockHubItem.unlock();


}

