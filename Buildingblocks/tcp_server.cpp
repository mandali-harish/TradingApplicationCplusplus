#include "tcp_server.h"
#include "socket_utils.h"

namespace Common 
{
    auto TCPServer::epoll_add(TCPSocket *socket)
    {
        epoll_event ev{};
        ev.events = EPOLLET | EPOLLIN;
        ev.data.ptr = reinterpret_cast<void *>(socket);
        return(epoll_ctl(efd_, EPOLL_CTL_ADD, socket->fd_, &ev) != -1);
    }

    auto TCPServer::listen(const std::string &iface, int port) -> void
    {
        destroy();
        efd_ = epoll_create(1);
        ASSERT(efd_ >= 0, "epoll_create() failed error:" + std::string(std::strerror(errno)));
        ASSERT(listener_socket_.connect("", iface, port, true) >=0, "Listener socket failed to connect. iface:" + iface + "port:" + std::to_string(port) + "error:" + std::string(std::strerror(errno)));
        ASSERT(epoll_add(&listener_socket_), "epoll_ctl() failed. error:" + std::string(std::strerror(errno)));

    }

    auto TCPServer::epoll_del(TCPSocket *socket)
    {
        return(epoll_ctl(efd_, EPOLL_CTL_DEL, socket->fd_, nullptr)!= -1);
    }

    auto TCPServer::del(TCPSocket *socket)
    {
        epoll_del(socket);
        sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket), sockets_.end());
        receive_sockets_.erase(std::remove(receive_sockets_.begin(), receive_sockets_.end(), socket), receive_sockets_.end());
        send_sockets_.erase(std::remove(send_sockets_.begin(), send_sockets_.end(), socket), send_sockets_.end());
    }

    auto TCPServer::poll() noexcept -> void
    {
        const int max_events = 1 + sockets_.size();
        for(auto socket:disconnected_sockets_)
        {
            del(socket);
        }
        const int n = epoll_wait(efd_, events_, max_events, 0);

        bool have_new_connection = false;
        for(int i=0; i<n; ++i)
        {
            epoll_event &event = events_[i];
            auto socket = reinterpret_cast<TCPSocket *>(event.data.ptr);

            if(event.events  & EPOLLIN)
            {
                if(socket == &listener_socket_)
                {
                    logger_.log("%:% %() % EPOLLIN listener_socket:%\n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->fd_);
                    have_new_connection = true;
                    continue;
                }
                logger_.log("%:% %() % EPOLLIN socket:%\n",
                            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->fd_);
                if(std::find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
                    receive_sockets_.push_back(socket);
            }

            if(event.events & EPOLLOUT)
            {
                logger_.log("%:% %() % EPOLLOUT socket:%\n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->fd_);
                
                if(std::find(send_sockets_.begin(), send_sockets_.end(), socket) == send_sockets_.end())
                    send_sockets_.push_back(socket);
            }

            if(event.events & (EPOLLERR | EPOLLHUP))
            {
                logger_.log("%:% %() % EPOLLERR socket:%\n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->fd_);
                if(std::find(disconnected_sockets_.begin(),disconnected_sockets_.end(), socket) == disconnected_sockets_.end())
                    disconnected_sockets_.push_back(socket);
            }
        }
        while(have_new_connection)
        {
            logger_.log("%:% %() % have_new_connction\n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
            sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);
            int fd = accept(listener_socket_.fd_, reinterpret_cast<sockaddr *>(&addr), &addr_len);
            if(fd == -1)
                break;
            ASSERT(setNonBlocking(fd) && setNoDelay(fd), "Failed to set non-blocking or no delay on socket:" + std::to_string(fd));
            logger_.log("%:% %() % accepted socket:%\n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), fd);

            TCPSocket *socket = new TCPSocket(logger_);
            socket->fd_ = fd;
            socket->recv_callback_ = recv_callback_;
            ASSERT(epoll_add(socket), "Unable to add socket. error:" + std::string(std::strerror(errno)));
            if(std::find(sockets_.begin(), sockets_.end(), socket) == sockets_.end())
                sockets_.push_back(socket);
            if(std::find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
                receive_sockets_.push_back(socket);
        }
    }

    auto TCPServer::sendAndRecv() noexcept -> void
    {
        auto recv = false;
        for(auto socket: receive_sockets_)
        {
            if(socket->sendAndRecv())
                recv = true;
        }
        if(recv)
            recv_finished_callback_();
        
        for(auto socket: send_sockets_)
        {
            socket->sendAndRecv();
        }
    }
}