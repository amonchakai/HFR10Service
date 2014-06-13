/*
 * PrivateMessageController.hpp
 *
 *  Created on: 29 mars 2014
 *      Author: PierreL
 */

#ifndef PRIVATEMESSAGECONTROLLER_HPP_
#define PRIVATEMESSAGECONTROLLER_HPP_


#include <QtCore/QObject>
#include <bb/cascades/ListView>

class PrivateMessageListItem;

#ifndef RENDERING_OPTION
#define RENDERING_OPTION
enum RenderOption {
    IN_APP = 1,
    IN_HUB = 2
};

#endif

class PrivateMessageController : public QObject {
	Q_OBJECT;

	private:

		bb::cascades::ListView  		 *m_ListView;
		QList<PrivateMessageListItem*>   *m_Datas;
		QString                           m_HashCheck;
		RenderOption                      m_RenderOption;


	// ----------------------------------------------------------------------------------------------
	public:
		PrivateMessageController(QObject *parent = 0);
		virtual ~PrivateMessageController() {};

		inline void     setRenderLocation(RenderOption render)  { m_RenderOption = render; }

	// ----------------------------------------------------------------------------------------------

	public Q_SLOTS:
		inline void setListView	   (QObject *listView) 		{m_ListView = dynamic_cast<bb::cascades::ListView*>(listView); }
		void getMessages();
		inline const QList<PrivateMessageListItem*>    *getMessageList() const          { return m_Datas; };
		void checkReply();

		void deletePrivateMessage(const QString &url);
		void checkMessageDeleted();


	// ----------------------------------------------------------------------------------------------
	Q_SIGNALS:
		void complete();
		void autentificationFailed();




	// ----------------------------------------------------------------------------------------------
	private:

		void parse(const QString &page);
		void parseMessageListing(bool read, const QString &threadListing);
		void updateView();

		void checkErrorMessage(const QString &page);
		void connectionTimedOut();
};



#endif /* PRIVATEMESSAGECONTROLLER_HPP_ */
