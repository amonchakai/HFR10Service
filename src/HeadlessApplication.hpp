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

#ifndef HEADLESS_APPLICATION
#define HEADLESS_APPLICATION

#include <bb/system/InvokeManager>
#include <bb/Application>
#include <QObject>
#include <QReadWriteLock>

#include "PrivateMessageController.hpp"
#include "ListFavoriteController.hpp"
#include "Settings.hpp"

namespace bb
{
    namespace cascades
    {
        class LocaleHandler;
    }
}

class QTranslator;
class QTimer;
class HubIntegration;

struct HubItem {
    qint64      m_HubId;
    QString     m_ThreadId;
    QString     m_Timestamp;
};

/*!
 * @brief Application UI object
 *
 * Use this object to create and init app UI, to create context objects, to register the new meta types etc.
 */
class HeadlessApplication : public QObject
{
    Q_OBJECT

private:
    QTranslator                                     *m_pTranslator;
    bb::cascades::LocaleHandler                     *m_pLocaleHandler;

    bb::system::InvokeManager                       *m_InvokeManager;
    bb::Application                                 *m_app;

    QTimer                                          *m_Timer;
    PrivateMessageController                        *m_PrivateMessageController;
    ListFavoriteController                          *m_ListFavoriteController;
    Settings                                        *m_AppSettings;

    HubIntegration                                  *m_Hub;
    QList<HubItem>                                   m_ItemsInHub;

    QReadWriteLock                                   m_LockHubItem;

public:
    HeadlessApplication(bb::Application *app);
    virtual ~HeadlessApplication() {}




private slots:
    void onSystemLanguageChanged();
    void onInvoked(const bb::system::InvokeRequest& request);
    void periodicCheck();
    void insertNewEntries();
    void insertNewFavEntries();
    void checkLogin();


    void markHubItemRead(QVariantMap itemProperties);
    void markHubItemUnread(QVariantMap itemProperties);
    void removeHubItem(QVariantMap itemProperties);

};

#endif /* HEADLESS_APPLICATION */
