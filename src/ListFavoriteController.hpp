/*
 * ListFavoriteController.hpp
 *
 *  Created on: 15 mars 2014
 *      Author: PierreL
 */

#ifndef LISTFAVORITECONTROLLER_HPP_
#define LISTFAVORITECONTROLLER_HPP_


#include <QtCore/QObject>
#include <bb/cascades/ListView>

class ThreadListItem;

#ifndef RENDERING_OPTION
#define RENDERING_OPTION
enum RenderOption {
    IN_APP = 1,
    IN_HUB = 2
};

#endif

class ListFavoriteController : public QObject {
	Q_OBJECT;

	private:

		bb::cascades::ListView   *m_ListView;
		QList<ThreadListItem*>   *m_Datas;
		QString                   m_HashCheck;
		RenderOption              m_RenderOption;

	// ----------------------------------------------------------------------------------------------
	public:
	    ListFavoriteController(QObject *parent = 0);
		virtual ~ListFavoriteController() {};

		inline void     setRenderLocation(RenderOption render)                   { m_RenderOption = render; }
		inline const QList<ThreadListItem*>    *getFavoriteList() const          { return m_Datas; };

	// ----------------------------------------------------------------------------------------------

	public Q_SLOTS:
		inline void setListView	   (QObject *listView) 		{m_ListView = dynamic_cast<bb::cascades::ListView*>(listView); }
		void getFavorite();
		void checkReply();
		void checkReplyDeleteFlag();
		void updateView();
		void deleteFlag(const QString &url);


	// ----------------------------------------------------------------------------------------------
	Q_SIGNALS:
		void complete();




	// ----------------------------------------------------------------------------------------------
	private:

		void parse(const QString &page);
		void parseThreadListing(const QString &category, const QString &caption, const QString &urlFirstPage, const QString &threadListing, const QString &today);


		void checkErrorMessage(const QString &page);
		void connectionTimedOut();

};


#endif /* LISTFAVORITECONTROLLER_HPP_ */
