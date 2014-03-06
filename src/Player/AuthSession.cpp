#include "AuthSession.hpp"

void AuthSession::start()
{
    player_list_.join(shared_from_this());
    sLog.outString("A new client connected : %s", socket_.remote_endpoint().address().to_string().c_str());
    
    boost::asio::async_read_until(socket_, handshake_buffer_, "\r\n\r\n",
        boost::bind(&AuthSession::parseHanshake, this,
            boost::asio::placeholders::error));
    
}

void AuthSession::deliver(const WebSocketMessage& msg)
{
    /*bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
        ByteBuffer packet = write_msgs_.front().getPacket();
        boost::asio::async_write(socket_,
            boost::asio::buffer(packet.contents(),
                packet.size()),
                    boost::bind(&AuthSession::handle_write, shared_from_this(),
                        boost::asio::placeholders::error));
    }*/
}

void AuthSession::decodeHeader(const boost::system::error_code& error){
    if (!error)
    {
    ByteBuffer packet;
    received_message_ = new WebSocketMessage(BINARY_MESSAGE, 1); //Creating the new packet
    packet.appendData(buffer_, 2);
    
    uint8 opCode = 0, length = 0;
    
    packet >> opCode; //Get the opcode
    received_message_->SetOpcode(opCode);
        
    if(received_message_->GetOpcode() != 130) //Check if it's the right opcode
    {
        player_list_.remove(shared_from_this());
        return;
    }
        
    packet >> length; //Get the length
    length &= 127; //Remove the first bit
    received_message_->setSize(length);
    
    if(length<=125){ //read the mask ( 4 bytes ) + the additionnal length if needed
        received_message_->SetMaskIndex(0);
        boost::asio::async_read(socket_,
                                boost::asio::buffer(buffer_, 4),
                                boost::bind(&AuthSession::decodeMask, shared_from_this(),
                                            boost::asio::placeholders::error));
    }
    else if(length == 126){
        received_message_->SetMaskIndex(2);
        boost::asio::async_read(socket_,
                                boost::asio::buffer(buffer_, 6),
                                boost::bind(&AuthSession::decodeMask, shared_from_this(),
                                            boost::asio::placeholders::error));
    }
    else{ // 127
        received_message_->SetMaskIndex(8);
        boost::asio::async_read(socket_,
                                boost::asio::buffer(buffer_, 12),
                                boost::bind(&AuthSession::decodeMask, shared_from_this(),
                                            boost::asio::placeholders::error));
    }
    }
    else{
        player_list_.remove(shared_from_this());
    }
}

void AuthSession::decodeMask(const boost::system::error_code& error){
    if (!error)
    {
        int maskIndex = received_message_->GetMaskIndex();
        ByteBuffer mask;
        mask.appendData(buffer_, maskIndex+4);
     
        //fix length
        if(maskIndex == 2){
            uint16 length = 0;
            mask >> length;
            received_message_->setSize(length);
        }
        else if(maskIndex == 8){
            uint64 length = 0;
            mask >> length;
            received_message_->setSize(length);
        }

        //Set max size
        if(received_message_->getSize()>1024)
        {
            player_list_.remove(shared_from_this());
            return;
        }
    
        //get mask
        static uint8 maskArray[3];
        for(int i=0;i<4;i++){
            uint8 value = 0;
            mask >> value;
            maskArray[i] = value;
        }
        received_message_->setMask(maskArray);

        //Read the actual data
        boost::asio::async_read(socket_,
                                boost::asio::buffer(buffer_, received_message_->getSize()),
                                boost::bind(&AuthSession::decodeData, shared_from_this(),
                                            boost::asio::placeholders::error));
    
    }
    else{
        player_list_.remove(shared_from_this());
    }
}

void AuthSession::decodeData(const boost::system::error_code& error){
    if (!error)
    {
        uint8 size=received_message_->getSize(), *mask = received_message_->getMask(), byte=0;
        char message[size];
        ByteBuffer extractor;
        
        //Extract and decrypt received data
        extractor.appendData(buffer_, received_message_->getSize());
        for(int i = 0;i<size;i++)
        {
            extractor >> byte;
            message[i] = byte ^ mask[(i)%4];
        }
        
        //Form packet
        AuthMessage packet;
        packet.appendData(message, received_message_->getSize());
        packet.set_opcode_(packet.read<uint8>());
        packet.set_length_(received_message_->getSize() - 1);
        
        OpcodeHandler const& opHandle = sOpcodeTable[packet.getOpcode()];
        (this->*opHandle.handler)(packet);
        
        delete received_message_; //Freeee bird
        
        //Loop, reading new data :)
        boost::asio::async_read(socket_,
                                boost::asio::buffer(buffer_, 2),
                                boost::bind(&AuthSession::decodeHeader, shared_from_this(),
                                            boost::asio::placeholders::error));

    }
    else{
        player_list_.remove(shared_from_this());
    }
}

void AuthSession::parseHanshake(const boost::system::error_code& error){
    if(!error) {
        std::string command;
        std::istream is(&handshake_buffer_);
        
        getline(is, command);

        //If using wrong HTTP version
        if(command != "GET / HTTP/1.1\r")
        {
            sLog.outWarning("A nasty guy tried to connect outside of a browser or is using internet explorer");
            player_list_.remove(shared_from_this());
            return;
        }
        
        HandshakeParser parser(is);
        std::string handshakeSecKey = parser.getValue("Sec-WebSocket-Key");
        
        if(handshakeSecKey == "unknown"){
            player_list_.remove(shared_from_this());
            return;
        }
        
        std::string secWebSocketAccept = parser.getWebSocketAcceptKey(handshakeSecKey);
        
        std::string packet = parser.getHandshakeAnswer(secWebSocketAccept);
        boost::asio::async_write(socket_,
            boost::asio::buffer(packet,
                packet.length()),
                    boost::bind(&AuthSession::handle_write, shared_from_this(),
                        boost::asio::placeholders::error));
        
        boost::asio::async_read(socket_,
            boost::asio::buffer(buffer_, 2),
                boost::bind(&AuthSession::decodeHeader, shared_from_this(),
                    boost::asio::placeholders::error));
    }
    else
    {
        player_list_.remove(shared_from_this());
    }
}

void AuthSession::handle_write(const boost::system::error_code& error)
{
    if (!error){
    }
    else
    {
        player_list_.remove(shared_from_this());
    }
}

void AuthSession::handleLoginChallenge(AuthMessage& recvPacket){
    std::string username, password;
    recvPacket >> username;
    recvPacket >> password;
    sLog.outString("Received login username : %s password : %s", username.c_str(), password.c_str());
}

void AuthSession::handleRegisterChallenge(AuthMessage& recvPacket){
    std::string username, password, email;
    recvPacket >> username;
    recvPacket >> password;
    recvPacket >> email;
    sLog.outString("Received register username : %s password : %s email : %s", username.c_str(), password.c_str(), email.c_str());
}

void AuthSession::handleNull(AuthMessage& recvPacket){
    
}