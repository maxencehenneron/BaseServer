#ifndef _AUTHSERVER_H
#define _AUTHSERVER_H

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <cppconn/resultset.h>
#include "../Utils/Types.hpp"
#include "../Player/AuthSession.hpp"
#include "../Database/DatabaseQuery.hpp"

using boost::asio::ip::tcp;

class AuthServer
{
public:
  AuthServer(boost::asio::io_service& io_service,
      const tcp::endpoint& endpoint)
    : io_service_(io_service),
      acceptor_(io_service, endpoint)
  {
    start_accept();
  }

    void start_accept();
    void handle_accept(auth_session_ptr session, const boost::system::error_code& error);
	void initGameAccounts();
	std::map<int,game_account>* getAccountList(){ return &accounts_map; }
private:
    void loadFriendships();

    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
    std::map<int,game_account> accounts_map; 
    std::map<int,friendship> friendships_map;
    ClientList clients;
    PlayerList players;
};

typedef boost::shared_ptr<AuthServer> AuthServer_ptr;

#endif