/*
 * CookieJar.h
 *
 *  Created on: 15 mars 2014
 *      Author: PierreL
 */

#ifndef COOKIEJAR_H_
#define COOKIEJAR_H_

#include <QNetworkCookieJar>

class CookieJar : public QNetworkCookieJar {
	Q_OBJECT;


public:
	// ----------------------------------------------------------------------------------------------
	// member functions
	static CookieJar* get();

	// singleton; hide constructor
    CookieJar(QObject *parent = NULL);
	virtual ~CookieJar();


	void saveToDisk();
	void loadFromDisk();
	bool areThereCookies();
	void deleteCookies();


private:

	// ----------------------------------------------------------------------------------------------
	// member variables
	static CookieJar *m_This;




};

#endif /* COOKIEJAR_H_ */
