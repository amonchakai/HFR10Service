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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QRegExp>
#include <QDateTime>

#include "Hub/HubIntegration.hpp"
#include "Hub/HubCache.hpp"

#include "DataObjects.h"
#include "Network/HFRNetworkAccessManager.hpp"
#include "Network/CookieJar.hpp"
#include "Globals.h"

using namespace bb::cascades;

bool HeadlessApplication::m_Logs = false;

HeadlessApplication::HeadlessApplication(bb::Application *app) :
        QObject(app),
        m_InvokeManager(new bb::system::InvokeManager()),
        m_app(app),
        m_UdsUtil(NULL),
        m_Settings(NULL),
        m_HubCache(NULL),
        m_Hub(NULL) {

    m_InvokeManager->setParent(this);

    // ---------------------------------------------------------------------
    // prepare to process events

    qDebug() << "-----------------------------------\nStart Headless app!...\n------------------------------------";



    qDebug() << "initializeHub()";
    initializeHub();

    // ---------------------------------------------------------------------
    // Catch events

    qDebug() << "connect invoke framework";
    bool connectResult;

    Q_UNUSED(connectResult);
    connectResult = connect( m_InvokeManager,
             SIGNAL(invoked(const bb::system::InvokeRequest&)),
             this,
             SLOT(onInvoked(const bb::system::InvokeRequest&)));

    Q_ASSERT(connectResult);

}

void HeadlessApplication::onInvoked(const bb::system::InvokeRequest& request) {
    qDebug() << "invoke Headless!" << request.action();
    initializeHub();

    if(request.action().compare("bb.action.system.STARTED") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : auto started";
        } else if(request.action().compare("bb.action.START") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : start";
            resynchHub();
        } else if(request.action().compare("bb.action.DELETE.ACCOUNT") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: DELETE ACCOUNT CALLED";
            m_Hub->remove();

        } else if(request.action().compare("bb.action.system.TIMER_FIRED") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: bb.action.system.TIMER_FIRED";
            resynchHub();

        } else if(request.action().compare("bb.action.STOP") == 0) {
            qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : stop";
            m_app->requestExit();

        } else if(request.action().compare("bb.action.MARKREAD") == 0) {
            //qDebug() << "HeadlessHubIntegration: onInvoked: mark read" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            {

                markHubItemRead(attributesMap);
            }


        } else if(request.action().compare("bb.action.MARKUNREAD") == 0) {
            //qDebug() << "HeadlessHubIntegration: onInvoked: mark unread" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            {
                markHubItemUnread(attributesMap);
            }

        } else if(request.action().compare("bb.action.MARKPRIORREAD") == 0) {
            bb::data::JsonDataAccess jda;

            qint64 timestamp = (jda.loadFromBuffer(request.data())).toLongLong();

            QVariantList hubItems = m_Hub->items();
            for(int i = 0 ; i < hubItems.size() ; ++i) {
                QVariantMap item = hubItems.at(i).toMap();

                if(item["timestamp"].toLongLong() < timestamp) {

                    markHubItemRead(item);
                }
            }


        } else if(request.action().compare("bb.action.DELETE") == 0) {
            // qDebug() << "HeadlessHubIntegration: onInvoked: HeadlessHubIntegration : delete" << request.data();
            bb::data::JsonDataAccess jda;

            QVariantMap objectMap = (jda.loadFromBuffer(request.data())).toMap();
            QVariantMap attributesMap = objectMap["attributes"].toMap();

            {
                removeHubItem(attributesMap);
            }

        } else if(request.action().compare("bb.action.DELETEPRIOR") == 0) {
            bb::data::JsonDataAccess jda;

            qint64 timestamp = (jda.loadFromBuffer(request.data())).toLongLong();
            QVariantList hubItems = m_Hub->items();
            for(int i = 0 ; i < hubItems.size() ; ++i) {
                QVariantMap item = hubItems.at(i).toMap();

                if(item["timestamp"].toLongLong() < timestamp) {
                    removeHubItem(item);
                }
            }


            qDebug() << "HeadlessHubIntegration: onInvoked: mark prior delete : " << timestamp << " : " << request.data();

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

    m_Hub->markHubItemRead(m_Hub->categoryId(), itemId);
}

void HeadlessApplication::markHubItemUnread(QVariantMap itemProperties) {

    qint64 itemId;

    if (itemProperties["sourceId"].toString().length() > 0) {
        itemId = itemProperties["sourceId"].toLongLong();
    } else if (itemProperties["messageid"].toString().length() > 0) {
        itemId = itemProperties["messageid"].toLongLong();
    }

    m_Hub->markHubItemUnread(m_Hub->categoryId(), itemId);
}

void HeadlessApplication::removeHubItem(QVariantMap itemProperties) {

    qint64 itemId;
    if (itemProperties["sourceId"].toString().length() > 0) {
        itemId = itemProperties["sourceId"].toLongLong();
    } else if (itemProperties["messageid"].toString().length() > 0) {
        itemId = itemProperties["messageid"].toLongLong();
    }

    m_Hub->removeHubItem(m_Hub->categoryId(), itemId);
}


void HeadlessApplication::initializeHub() {
//    return;

    m_InitMutex.lock();

    // initialize UDS
    if (!m_UdsUtil) {
        qDebug() << "new UDSUtil()";
        m_UdsUtil = new UDSUtil(QString("HFR10HubService"), QString("hubassets"));
    }

    if (!m_UdsUtil->initialized()) {
        qDebug() << "initialize()";
        m_UdsUtil->initialize();
    }

    if (m_UdsUtil->initialized() && m_UdsUtil->registered()) {
        qDebug() << "m_UdsUtil->initialized() && m_UdsUtil->registered()";
        if (!m_Settings) {

            m_Settings = new QSettings("Amonchakai", "HFR10Service");
        }
        if (!m_HubCache) {
            m_HubCache = new HubCache(m_Settings);
        }
        if (!m_Hub) {
            qDebug() << "new HubIntegration()";
            m_Hub = new HubIntegration(m_UdsUtil, m_HubCache);
        }
    }

    qDebug() << "done initializeHub()";
    m_InitMutex.unlock();
}


// ------------------------------------------------------------------------------------
// List all conversations, and update the Hub

void HeadlessApplication::resynchHub() {

    if(m_Hub == NULL) {
        return;
    }

    QSettings settings("Amonchakai", "HFR10");
    m_Logs = settings.value("LogEnabled", false).toBool();

    qDebug() << "Headless Logs: " << m_Logs;

    getPrivateMessages();


}








// =================================================================================================
// Private messages stuff


void HeadlessApplication::getPrivateMessages() {

    // list green + yellow flags
    const QUrl url(DefineConsts::FORUM_URL + "/forum1.php?config=hfr.inc&cat=prive&owntopic=0");


    CookieJar *cookies = new CookieJar();
    cookies->loadFromDisk();
    QNetworkAccessManager *accessManager = new QNetworkAccessManager();
    accessManager->setCookieJar(cookies);


    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");



    QNetworkReply* reply = accessManager->get(request);
    bool ok = connect(reply, SIGNAL(finished()), this, SLOT(checkReply()));
    Q_ASSERT(ok);
    Q_UNUSED(ok);

}


void HeadlessApplication::checkReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    QString response;
    if (reply) {
        if (reply->error() == QNetworkReply::NoError) {
            const int available = reply->bytesAvailable();
            if (available > 0) {
                const QByteArray buffer(reply->readAll());
                response = QString::fromUtf8(buffer);
                if(checkErrorMessage(response))
                    parse(response);
            }

            getFavoriteThreads();
        }

        reply->deleteLater();
    }
}

bool HeadlessApplication::checkErrorMessage(const QString &page) {

    QRegExp message("vous ne faites pas partie des membres ayant");
    message.setCaseSensitivity(Qt::CaseSensitive);
    message.setMinimal(true);

    if(message.indexIn(page, 0) != -1) {
        qDebug()  << ("You are not logged in.");
        return false;
    }

    return true;
}

void HeadlessApplication::parse(const QString &page) {

    QRegExp andAmp("&amp;");
    QRegExp quote("&#034;");
    QRegExp euro("&euro;");
    QRegExp inf("&lt;");
    QRegExp sup("&gt;");




    if(m_Logs) {
        QVariantList hubItems = m_Hub->items();
        QVariantMap itemMap;

        qDebug() << "Items in the Hub";
        for(int i = 0 ; i < hubItems.length() ; ++i) {
            itemMap = hubItems.at(i).toMap();
            qDebug() << "id: " << itemMap["sourceId"].toLongLong() << " timestamp " <<  itemMap["timestamp"].toLongLong() << " readCount " << itemMap["readCount"].toInt();
        }
    }

    // ----------------------------------------------------------------------------------------------
    // Parse unread MP using regexp

    QRegExp regexp("<td class=\"sujetCase1 cBackCouleurTab[0-9] \"><img src=\".*\" title=\".*\" alt=\"(Off|On)\" /></td>"); // topic read?

    regexp.setCaseSensitivity(Qt::CaseSensitive);
    regexp.setMinimal(true);


    int pos = 0;
    int lastPos = regexp.indexIn(page, pos);
    bool read = regexp.cap(1).compare("On") == 0;

    while((pos = regexp.indexIn(page, lastPos)) != -1) {
        pos += regexp.matchedLength();
        // parse each post individually
        parseMessageListing(read, page.mid(lastPos, pos-lastPos));


        read = regexp.cap(1).compare("On") == 0;
        lastPos = pos;
    }
    parseMessageListing(read, page.mid(lastPos, pos-lastPos));

}

void HeadlessApplication::parseMessageListing(bool read, const QString &threadListing) {
    PrivateMessageListItem *item = new PrivateMessageListItem();

    QRegExp andAmp("&amp;");
    QRegExp quote("&#034;");
    QRegExp euro("&euro;");
    QRegExp inf("&lt;");
    QRegExp sup("&gt;");
    QRegExp nbsp("&nbsp;");

    QString SubjectNumber;

    item->setRead(read);

    //qDebug() << threadListing;

    QRegExp addresseeRegexp("<td class=\"sujetCase6 cBackCouleurTab[0-9] \"><a rel=\"nofollow\" href=\"[^\"]+pseudo=([^\"]+)\" class=\"Tableau\">([^\"]+)</a></td>");
    addresseeRegexp.setCaseSensitivity(Qt::CaseSensitive);
    //addresseeRegexp.setMinimal(true);

    if(addresseeRegexp.indexIn(threadListing, 0) != -1) {
        item->setAddressee(addresseeRegexp.cap(1));
        //qDebug() << addresseeRegexp.cap(1) << addresseeRegexp.cap(2) << addresseeRegexp.cap(0) ;
    } else {
        item->deleteLater();
        return;
    }

    QRegExp addresseeRead("<span class=\"red\" title");
    addresseeRead.setCaseSensitivity(Qt::CaseSensitive);
    addresseeRead.setMinimal(true);


    if(addresseeRead.indexIn(threadListing, 0) != -1)
        item->setAddresseeRead(false);
    else
        item->setAddresseeRead(true);


    QRegExp mpTitle("class=\"sujetCase3\".*<a href=\"(.+)\" class=\"cCatTopic\" title=\"Sujet n.([0-9]+)\">(.+)</a></td><td class=\"sujetCase4\">");
    mpTitle.setCaseSensitivity(Qt::CaseSensitive);
    mpTitle.setMinimal(true);


    if(mpTitle.indexIn(threadListing, 0) != -1) {
        QString s = mpTitle.cap(3);
        SubjectNumber = mpTitle.cap(2);
        s.replace(andAmp, "&");
        s.replace(quote,"\"");
        s.replace(euro, "e");
        s.replace(inf, "<");
        s.replace(sup, ">");

        item->setTitle(s);

        s = mpTitle.cap(1);
        s.replace(andAmp, "&");
        item->setUrlFirstPage(s);
    }


    QRegExp lastPostInfo("<td class=\"sujetCase9 cBackCouleurTab[0-9] \"><a href=\"(.+)\" class=\"Tableau\">(.+)<br /><b>(.+)</b></a></td><td class=\"sujetCase10\">");
    lastPostInfo.setCaseSensitivity(Qt::CaseSensitive);
    lastPostInfo.setMinimal(true);

    if(lastPostInfo.indexIn(threadListing, 0) != -1) {
        QString s = lastPostInfo.cap(1);
        s.replace(andAmp, "&");
        item->setUrlLastPage(s);

        s = lastPostInfo.cap(2);
        s.replace(nbsp, " ");
        item->setTimestamp(s);

        item->setLastAuthor(lastPostInfo.cap(3));
    }

    qint64 timestamp = QDateTime::fromString(item->getTimestamp().mid(0,10) + " " + item->getTimestamp().mid(13), "dd-MM-yyyy hh:mm").toMSecsSinceEpoch();

    // -----------------------------------------------------
    // items already into the hub
    QVariantList hubItems = m_Hub->items();
    QVariantMap itemMap;

    bool existing = false;
    for(int i = 0 ; i < hubItems.length() ; ++i) {
        itemMap = hubItems.at(i).toMap();

        if(itemMap["syncId"].toString() == SubjectNumber) {
            existing = true;
            break;
        }
    }



    qDebug() << "Current item: " << SubjectNumber << " existing: " << existing << " id: src:" << itemMap["sourceId"].toLongLong() << " msg: " << itemMap["messageid"].toString();

    if(existing) {

        qDebug() << "Current item time: " << itemMap["timestamp"].toULongLong() << " new time: " << timestamp;
        if(itemMap["timestamp"].toULongLong() == timestamp) {
            item->deleteLater();
            return;
        }

        qDebug() << "update item. isAddresseeRead: " << item->isAddresseeRead() << " isRead: "  << item->isRead();

        itemMap["description"] = item->getTitle();
        itemMap["timestamp"] = timestamp;
        itemMap["readCount"] = item->isRead();
        itemMap["url"] = item->getUrlLastPage();

        qint64 itemId;
        if (itemMap["sourceId"].toString().length() > 0) {
            itemId = itemMap["sourceId"].toLongLong();
        } else if (itemMap["messageid"].toString().length() > 0) {
            itemId = itemMap["messageid"].toLongLong();
        }

        if(item->isRead()) {
            itemMap["readCount"] = 0;
        }

        qDebug() << "item id: " << itemId;

        m_Hub->updateHubItem(m_Hub->categoryId(), itemId, itemMap, item->isRead());

        if(!item->isRead()) {
            qDebug() << "updated mark item as read.";
            m_Hub->markHubItemRead(m_Hub->categoryId(), itemId);
        }

    } else {

        qDebug() << "new item!";
        QVariantMap entry;
        if(!item->getUrlLastPage().isEmpty())
            entry["url"] = item->getUrlLastPage();
        else {
            if(!item->getUrlFirstPage().isEmpty())
                entry["url"] = item->getUrlFirstPage();
            else {
                item->deleteLater();
                return;
            }
        }

        qDebug() << "insert item into hub";
        m_Hub->addHubItem(m_Hub->categoryId(), entry, item->getAddressee(), item->getTitle(), timestamp, SubjectNumber, SubjectNumber, "",  item->isRead());

        qint64 itemId;
        if (entry["sourceId"].toString().length() > 0) {
            itemId = entry["sourceId"].toLongLong();
        } else if (entry["messageid"].toString().length() > 0) {
            itemId = entry["messageid"].toLongLong();
        }

        qDebug() << "new item id: " << itemId;
        if(!item->isRead()) {
            qDebug() << "mark new item as already read.";
            m_Hub->markHubItemRead(m_Hub->categoryId(), itemId);
        }
    }

    item->deleteLater();


}



void HeadlessApplication::getFavoriteThreads() {

    // list green + yellow flags
    const QUrl url(DefineConsts::FORUM_URL + "/forum1f.php?owntopic=1");

    qDebug() << "getFavoriteThreads()";
    CookieJar *cookies = new CookieJar();
    cookies->loadFromDisk();
    QNetworkAccessManager *accessManager = new QNetworkAccessManager();
    accessManager->setCookieJar(cookies);


    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");



    QNetworkReply* reply = accessManager->get(request);
    bool ok = connect(reply, SIGNAL(finished()), this, SLOT(checkReplyFav()));
    Q_ASSERT(ok);
    Q_UNUSED(ok);
}

void HeadlessApplication::checkReplyFav() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    QString response;
    if (reply) {
        if (reply->error() == QNetworkReply::NoError) {
            const int available = reply->bytesAvailable();
            if (available > 0) {
                const QByteArray buffer(reply->readAll());
                response = QString::fromUtf8(buffer);
                if(checkErrorMessage(response))
                    parseFav(response);

            }
        }

        reply->deleteLater();
    }
}




void HeadlessApplication::parseFav(const QString &page) {

    QRegExp andAmp("&amp;");
    QRegExp quote("&#034;");
    QRegExp euro("&euro;");
    QRegExp inf("&lt;");
    QRegExp sup("&gt;");

    loadTags();


    // ----------------------------------------------------------------------------------------------
    // Parse categories using regexp

    // get categories name & position into the stream
    QRegExp regexp("<th class=\"padding\".*class=\"cHeader\"?>(.+)</a></th>"); // match categories' name
    regexp.setCaseSensitivity(Qt::CaseSensitive);
    regexp.setMinimal(true);


    QList<int> indexCategories;


    m_CategoriesLabels.clear();
    int pos = 0;
    while((pos = regexp.indexIn(page, pos)) != -1) {
        pos += regexp.matchedLength();
        indexCategories.push_back(pos);                 // Store position of each category into the stream

        QString s(regexp.cap(1)); s.replace(andAmp, "&");
        m_CategoriesLabels.push_back(s);        // Store the matching label

    }

    // Get favorite topics
    regexp = QRegExp(QString("<td class=\"sujetCase1 cBackCouleurTab[0-9] \"><img src=\".*\" title=\".*\" alt=\"(Off|On)\" /></td>.*<a href=\"(.+)\" class=\"cCatTopic\" title=\"Sujet n.[0-9]+\">(.+)</a></td>"));     // topics' name



    regexp.setCaseSensitivity(Qt::CaseSensitive);
    regexp.setMinimal(true);


    QString today = QDateTime::currentDateTime().toString("dd-MM-yyyy");


    pos = 0;
    int catIndex = 0;
    int lastPos = regexp.indexIn(page, pos);
    QString caption;
    QString category;
    int     groupKey = 0;
    QString urlFirstPage;

    if(lastPos != -1) {
        caption = regexp.cap(3);
        caption.replace(andAmp,"&");
        caption.replace(quote,"\"");
        caption.replace(euro, "e");
        caption.replace(inf, "<");
        caption.replace(sup, ">");

        urlFirstPage = regexp.cap(2);
        urlFirstPage.replace(andAmp, "&");

        lastPos += regexp.matchedLength();
        for( ; catIndex < indexCategories.length() && lastPos > indexCategories[catIndex] ; ++catIndex) {}
        category = m_CategoriesLabels[catIndex-1];
        groupKey = catIndex-1;

    }

    while((pos = regexp.indexIn(page, lastPos)) != -1) {
        pos += regexp.matchedLength();

        // parse each post individually
        parseThreadListing(category, caption, urlFirstPage, page.mid(lastPos, pos-lastPos), today, groupKey);

        catIndex = 0;
        for( ; catIndex < indexCategories.length() && pos > indexCategories[catIndex] ; ++catIndex) {}
        category = m_CategoriesLabels[catIndex-1];
        groupKey = catIndex-1;

        lastPos = pos;
        caption = regexp.cap(3);
        caption.replace(andAmp,"&");
        caption.replace(quote,"\"");
        caption.replace(euro, "e");
        caption.replace(inf, "<");
        caption.replace(sup, ">");

        urlFirstPage = regexp.cap(2);
        urlFirstPage.replace(andAmp, "&");
    }
    parseThreadListing(category, caption, urlFirstPage, page.mid(lastPos, pos-lastPos), today, groupKey);

}

void HeadlessApplication::parseThreadListing(const QString &category, const QString &caption, const QString &urlFirstPage, const QString &threadListing, const QString &today, int groupKey) {
    //                         + ".*<td class=\"sujetCase4\"><a href=\"(.+)\" class=\"cCatTopic\">"     // link to first post
    //                         + "([0-9]+)</a></td>"                                                    // overall number of pages
    //                         + ".*<td class=\"sujetCase5\"><a href=\"(.+)\"><img src"                 // index to last read post
    //                         + ".*p.([0-9]+)"                                     // last page read number
    //                         + ".*<td class=\"sujetCase9.*class=\"Tableau\">(.+)"                     // time stamp
    //                         + "<br /><b>(.+)</b></a></td><td class=\"sujetCase10\"><input type");    // last contributor

    QRegExp andAmp("&amp;");

    if(caption.isEmpty())
        return;

    ThreadListItem *item = new ThreadListItem();
    item->setCategory(category);
    item->setTitle(caption);
    item->setUrlFirstPage(urlFirstPage);

    QRegExp firstPostAndPage(QString("<td class=\"sujetCase4\"><a href=\".+\" class=\"cCatTopic\">")        // link to first post
                                 + "([0-9]+)</a></td>");                                                    // overall number of pages

    firstPostAndPage.setCaseSensitivity(Qt::CaseSensitive);
    firstPostAndPage.setMinimal(true);

    if(firstPostAndPage.indexIn(threadListing, 0) != -1) {
        item->setPages(firstPostAndPage.cap(1));
    }



    QRegExp lastReadAndPage(QString("<td class=\"sujetCase5\"><a href=\"(.+)\"><img src.*p.([0-9]+).*\" alt=\"flag\"")); // index to last read post + last page read number
    lastReadAndPage.setCaseSensitivity(Qt::CaseSensitive);
    lastReadAndPage.setMinimal(true);

    if(lastReadAndPage.indexIn(threadListing, 0) != -1) {
        QString s = lastReadAndPage.cap(1); s.replace(andAmp, "&");
        item->setUrlLastPostRead(s);

        item->setPages(lastReadAndPage.cap(2)  + "/" + item->getPages());
    }


    QRegExp lastContribution("<td class=\"sujetCase9 cBackCouleurTab[0-9] \"><a href=\"(.+)\" class=\"Tableau\">(.+)<br /><b>(.+)</b></a></td><td class=\"sujetCase10\"><input type"); // timestamp + last contributor
    lastContribution.setCaseSensitivity(Qt::CaseSensitive);
    lastContribution.setMinimal(true);

    if(lastContribution.indexIn(threadListing, 0) != -1) {
        QString s = lastContribution.cap(2);
        item->setDetailedTimestamp(s);

        item->setLastAuthor(lastContribution.cap(3));

        s = lastContribution.cap(1);
        s.replace(andAmp, "&");
        item->setUrlLastPage(s);

    }

    QRegExp postIDRegExp("post=([0-9]+)");
    QRegExp catIDRefExp("cat=([0-9]+)");
    if(postIDRegExp.indexIn(item->getUrlLastPage()) != -1 && catIDRefExp.indexIn(item->getUrlLastPage()) != -1) {
        item->setColor(getTagValue(postIDRegExp.cap(1) + "@" + catIDRefExp.cap(1)));
    } else {
        item->setColor(0);
    }

    //qDebug() << item->getTitle() << postIDRegExp.cap(1) + "@" + catIDRefExp.cap(1) << item->getColor();

    if(item->getColor() == 0) {
        item->deleteLater();
        return;
    }


    QSettings settings("Amonchakai", "HFR10");

    /*
    qDebug() << item->getColor()
             << settings.value("NotifGreen",false).toBool()
             << settings.value("NotifBlue",false).toBool()
             << settings.value("NotifOrange",false).toBool()
             << settings.value("NotifPink",false).toBool()
             << settings.value("NotifPurple",false).toBool();
     */
    switch(item->getColor()) {
        case 1:
            if(!settings.value("NotifGreen",false).toBool()){
                item->deleteLater();
                return;
            }
            break;

        case 2:
            if(!settings.value("NotifBlue",false).toBool()){
                item->deleteLater();
                return;
            }
            break;


        case 3:
            if(!settings.value("NotifOrange",false).toBool()){
                item->deleteLater();
                return;
            }
            break;

        case 4:
            if(!settings.value("NotifPink",false).toBool()){
                item->deleteLater();
                return;
            }
            break;

        case 5:
            if(!settings.value("NotifPurple",false).toBool()){
                item->deleteLater();
                return;
            }
            break;
    }



    QString SubjectNumber = postIDRegExp.cap(1) + QString::number(catIDRefExp.cap(1).toInt()*10000);
    qint64 timestamp = QDateTime::fromString(item->getDetailedTimestamp().mid(0,10) + " " + item->getDetailedTimestamp().mid(23), "dd-MM-yyyy hh:mm").toMSecsSinceEpoch();

    // -----------------------------------------------------------
    // add item into the Hub

    QVariantList hubItems = m_Hub->items();

    bool existing = false;
    QVariantMap itemMap;
    for(int i = 0 ; i < hubItems.length() ; ++i) {
        itemMap = hubItems.at(i).toMap();

        if(itemMap["syncId"].toString() == SubjectNumber) {
            existing = true;
            break;
        }
    }




    if(existing) {

        if(itemMap["timestamp"].toULongLong() == timestamp) {
            item->deleteLater();
            return;
        }

        itemMap["description"] = item->getLastAuthor();
        itemMap["timestamp"] = timestamp;
        itemMap["readCount"] = item->isRead();

        if(!item->getUrlLastPostRead().isEmpty())
            itemMap["url"] = item->getUrlLastPostRead();
        else {
            if(!item->getUrlLastPage().isEmpty())
                itemMap["url"] = item->getUrlFirstPage();
            else {
                itemMap["url"] = item->getUrlFirstPage();
            }
        }

        qint64 itemId;
        if (itemMap["sourceId"].toString().length() > 0) {
            itemId = itemMap["sourceId"].toLongLong();
        } else if (itemMap["messageid"].toString().length() > 0) {
            itemId = itemMap["messageid"].toLongLong();
        }

        qDebug() << "New favorite. update post --> notif.";
        m_Hub->updateHubItem(m_Hub->categoryId(), itemId, itemMap, true);

    } else {

        QVariantMap entry;
        if(!item->getUrlLastPostRead().isEmpty())
            entry["url"] = item->getUrlLastPostRead();
        else {
            if(!item->getUrlFirstPage().isEmpty())
                entry["url"] = item->getUrlFirstPage();
            else {
                item->deleteLater();
                return;
            }
        }

        qDebug() << "New favorite. add new entry --> notif.";
        m_Hub->addHubItem(m_Hub->categoryId(), entry, item->getTitle(), item->getLastAuthor(), timestamp, SubjectNumber, SubjectNumber, "",  true);

    }

    item->deleteLater();


}


int HeadlessApplication::getTagValue(const QString &topicID) {
    QMap<QString, int>::const_iterator it = m_TopicTags.find(topicID);
    if(it == m_TopicTags.end()) {
        return 0;
    }
    return it.value();
}

void HeadlessApplication::loadTags() {
    QString directory = QDir::homePath() + QLatin1String("/HFRBlackData");
    if (!QFile::exists(directory)) {
        return;
    }
    m_TopicTags.clear();

    QFile file(directory + "/NotificationList.txt");

    if (file.open(QIODevice::ReadOnly)) {

        QDataStream stream(&file);

        int nbTags = 0;
        stream >> nbTags;
        for(int i = 0 ; i < nbTags ; ++i) {
            QString key;
            int value;

            stream >> key;
            stream >> value;
            m_TopicTags[key] = value;
         }

        file.close();
    }
}



