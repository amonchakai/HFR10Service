#include "ListFavoriteController.hpp"


#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QRegExp>
#include <QDateTime>

#include <bb/cascades/AbstractPane>
#include <bb/cascades/GroupDataModel>
#include <bb/system/SystemToast>

#include  "Globals.h"
#include  "Network/HFRNetworkAccessManager.hpp"
#include  "DataObjects.h"
#include  "Settings.hpp"


ListFavoriteController::ListFavoriteController(QObject *parent)
	: QObject(parent), m_ListView(NULL), m_Datas(new QList<ThreadListItem*>()), m_RenderOption(IN_APP) {

}


void ListFavoriteController::deleteFlag(const QString &urlFirstPage) {

    QRegExp catIDRegExp("cat=([0-9]+)");
    if(catIDRegExp.indexIn(urlFirstPage) == -1)
        return;

    const QUrl url(DefineConsts::FORUM_URL + "/modo/manageaction.php?config=hfr.inc&cat=" + catIDRegExp.cap(1) + "&type_page=forum1&moderation=0");

    QRegExp postIDRegExp("post=([0-9]+)");
    if(postIDRegExp.indexIn(urlFirstPage) == -1)
        return;

    QRegExp ownRegExp("owntopic=([0-9]+)");
    if(ownRegExp.indexIn(urlFirstPage) == -1)
        return;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrl params;
    params.addQueryItem("action_reaction", "message_forum_delflags");
    params.addQueryItem("topic0", postIDRegExp.cap(1));
    params.addQueryItem("valuecat0", catIDRegExp.cap(1));
    params.addQueryItem("valueforum0", "hardwarefr");
    params.addQueryItem("type_page", "forum1");
    params.addQueryItem("owntopic", ownRegExp.cap(1));
    params.addQueryItem("topic1", "-1");
    params.addQueryItem("topic_statusno1", "-1");
    params.addQueryItem("hash_check", m_HashCheck);


    QNetworkReply* reply = HFRNetworkAccessManager::get()->post(request, params.encodedQuery());
    bool ok = connect(reply, SIGNAL(finished()), this, SLOT(checkReplyDeleteFlag()));
    Q_ASSERT(ok);
    Q_UNUSED(ok);
}

void ListFavoriteController::checkReplyDeleteFlag() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

        QString response;
        if (reply) {
            if (reply->error() == QNetworkReply::NoError) {
            } else {
                connectionTimedOut();
            }

            reply->deleteLater();
        }
}

void ListFavoriteController::getFavorite() {

	// list green + yellow flags
	const QUrl url(DefineConsts::FORUM_URL + "/forum1f.php?owntopic=1");

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");


	QNetworkReply* reply = HFRNetworkAccessManager::get()->get(request);
	bool ok = connect(reply, SIGNAL(finished()), this, SLOT(checkReply()));
	Q_ASSERT(ok);
	Q_UNUSED(ok);

}

void ListFavoriteController::checkErrorMessage(const QString &page) {

	QRegExp message("Aucun sujet que vous avez lu");
	message.setCaseSensitivity(Qt::CaseSensitive);
	message.setMinimal(true);

	QString error;

	if(message.indexIn(page, 0) != -1) {
		error = QString(tr("You are not logged in."));
	}

	if(error == "")
		return;

	if(m_RenderOption == IN_APP) {
        bb::system::SystemToast *toast = new bb::system::SystemToast(this);

        toast->setBody(error);
        toast->setPosition(bb::system::SystemUiPosition::MiddleCenter);
        toast->show();
	} else {
	    qDebug() << "ListFavoriteController -- Authentification may have failed...";
	}
}

void ListFavoriteController::checkReply() {
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	QString response;
	if (reply) {
		if (reply->error() == QNetworkReply::NoError) {
			const int available = reply->bytesAvailable();
			if (available > 0) {
				const QByteArray buffer(reply->readAll());
				response = QString::fromUtf8(buffer);
				checkErrorMessage(response);
				parse(response);
			}
		} else {
			connectionTimedOut();
		}

		reply->deleteLater();
	}
}

void ListFavoriteController::connectionTimedOut() {

    if(m_RenderOption == IN_APP) {
        bb::system::SystemToast *toast = new bb::system::SystemToast(this);

        toast->setBody(tr("Connection timed out"));
        toast->setPosition(bb::system::SystemUiPosition::MiddleCenter);
        toast->show();
    } else {
        qDebug() << "ListFavoriteController -- Connection time out...";
    }

	emit complete();
}


void ListFavoriteController::parse(const QString &page) {

	//for(int i = 0 ; i < m_Datas->length() ; ++i)
	//    if((*m_Datas)[i] != NULL)
	//        (*m_Datas)[i]->deleteLater();
	m_Datas->clear();

	QRegExp andAmp("&amp;");
	QRegExp quote("&#034;");
	QRegExp euro("&euro;");
	QRegExp inf("&lt;");
	QRegExp sup("&gt;");

	// ----------------------------------------------------------------------------------------------
	// Parse categories using regexp

	// get categories name & position into the stream
	QRegExp regexp("<th class=\"padding\".*class=\"cHeader\"?>(.+)</a></th>"); // match categories' name
	regexp.setCaseSensitivity(Qt::CaseSensitive);
	regexp.setMinimal(true);


	QList<int> indexCategories;
	QList<QString> categoriesLabels;

	int pos = 0;
	while((pos = regexp.indexIn(page, pos)) != -1) {
		pos += regexp.matchedLength();
		indexCategories.push_back(pos);					// Store position of each category into the stream

		QString s(regexp.cap(1)); s.replace(andAmp, "&");
		categoriesLabels.push_back(s);		// Store the matching label
	}


	// Get favorite topics
	regexp = QRegExp(QString("<td class=\"sujetCase1 cBackCouleurTab[0-9] \"><img src=\".*\" title=\".*\" alt=\"(Off|On)\" /></td>.*<a href=\"(.+)\" class=\"cCatTopic\" title=\"Sujet n.[0-9]+\">(.+)</a></td>"));  	// topics' name



	regexp.setCaseSensitivity(Qt::CaseSensitive);
	regexp.setMinimal(true);


	QString today = QDateTime::currentDateTime().toString("dd-MM-yyyy");


	pos = 0;
	int catIndex = 0;
	int lastPos = regexp.indexIn(page, pos);
	QString caption;
	QString category;
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
		category = categoriesLabels[catIndex-1];

	}

	while((pos = regexp.indexIn(page, lastPos)) != -1) {
		pos += regexp.matchedLength();

		// parse each post individually
		parseThreadListing(category, caption, urlFirstPage, page.mid(lastPos, pos-lastPos), today);

		catIndex = 0;
		for( ; catIndex < indexCategories.length() && pos > indexCategories[catIndex] ; ++catIndex) {}
		category = categoriesLabels[catIndex-1];

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
	parseThreadListing(category, caption, urlFirstPage, page.mid(lastPos, pos-lastPos), today);


	QRegExp hashCheckRegExp("name=\"hash_check\" value=\"([0-9a-zA-Z]+)\"");
	hashCheckRegExp.setCaseSensitivity(Qt::CaseSensitive);
	if(hashCheckRegExp.indexIn(page.mid(lastPos)) != -1) {
	    m_HashCheck = hashCheckRegExp.cap(1);
	}


	if(m_RenderOption == IN_APP)
	    updateView();

	emit complete();

}

void ListFavoriteController::parseThreadListing(const QString &category, const QString &caption, const QString &urlFirstPage, const QString &threadListing, const QString &today) {
	//						   + ".*<td class=\"sujetCase4\"><a href=\"(.+)\" class=\"cCatTopic\">"		// link to first post
	//						   + "([0-9]+)</a></td>"													// overall number of pages
	//						   + ".*<td class=\"sujetCase5\"><a href=\"(.+)\"><img src"					// index to last read post
	//						   + ".*p.([0-9]+)"										// last page read number
	//						   + ".*<td class=\"sujetCase9.*class=\"Tableau\">(.+)" 					// time stamp
	//						   + "<br /><b>(.+)</b></a></td><td class=\"sujetCase10\"><input type");	// last contributor

	QRegExp andAmp("&amp;");


	ThreadListItem *item = new ThreadListItem();
	item->setCategory(category);
	item->setTitle(caption);
	item->setUrlFirstPage(urlFirstPage);

	QRegExp firstPostAndPage(QString("<td class=\"sujetCase4\"><a href=\".+\" class=\"cCatTopic\">")		// link to first post
								 + "([0-9]+)</a></td>");													// overall number of pages

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
		if(s.mid(0,10).compare(today) == 0)
			item->setTimestamp(s.mid(23,5));
		else
			item->setTimestamp(s.mid(0,10));
		item->setDetailedTimestamp(s);


		item->setLastAuthor(lastContribution.cap(3));

		s = lastContribution.cap(1);
		s.replace(andAmp, "&");
		item->setUrlLastPage(s);

	}

	QRegExp postIDRegExp("post=([0-9]+)");
	QRegExp catIDRefExp("cat=([0-9]+)");
	if(postIDRegExp.indexIn(item->getUrlLastPage()) != -1 && catIDRefExp.indexIn(item->getUrlLastPage()) != -1) {
	    item->setColor(Settings::getTagValue(postIDRegExp.cap(1) + "@" + catIDRefExp.cap(1)));
	} else {
	    item->setColor(0);
	}

	m_Datas->append(item);

}


void ListFavoriteController::updateView() {
	// ----------------------------------------------------------------------------------------------
	// get the dataModel of the listview if not already available
	using namespace bb::cascades;


	if(m_ListView == NULL) {
		qWarning() << "did not received the listview. quit.";
		return;
	}

	GroupDataModel* dataModel = dynamic_cast<GroupDataModel*>(m_ListView->dataModel());
	if (dataModel) {
		dataModel->clear();
	} else {
		qDebug() << "create new model";
		dataModel = new GroupDataModel(
				QStringList() << "title"
							  << "timestamp"
							  << "lastAuthor"
							  << "urlFirstPage"
							  << "urlLastPage"
							  << "urlLastPostRead"
							  << "pages"
							  << "flagType"
							  << "read"
							  << "color"
				);
		m_ListView->setDataModel(dataModel);
	}
	dataModel->setGrouping(ItemGrouping::ByFullValue);

	// ----------------------------------------------------------------------------------------------
	// push data to the view

	QList<QObject*> datas;
	for(int i = m_Datas->length()-1 ; i >= 0 ; --i) {
		datas.push_back(m_Datas->at(i));
	}

	dataModel->clear();
	dataModel->insertList(datas);
}


