#ifndef incl_DRIVESHAFT_STATUS_LOOP_H_
#define incl_DRIVESHAFT_STATUS_LOOP_H_

#include <boost/asio.hpp>
#include "thread-registry.h"

namespace Driveshaft {

class StatusLoop {

public:
    StatusLoop(boost::asio::io_service& io_service, ThreadRegistryPtr registry);
    ~StatusLoop() = default;

private:
    void startAccept() noexcept;
    void handleAccept(const boost::system::error_code&) noexcept;
    void setDeadline() noexcept;
    void handleDeadline(const boost::system::error_code&) noexcept;

    StatusLoop() = delete;
    StatusLoop(const StatusLoop&) = delete;
    StatusLoop(StatusLoop&&) = delete;
    StatusLoop& operator=(const StatusLoop&) = delete;
    StatusLoop& operator=(const StatusLoop&&) = delete;

    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::deadline_timer m_deadline_timer;
    boost::asio::ip::tcp::socket m_socket;
    ThreadRegistryPtr m_registry;
};

} // namespace Driveshaft

#endif
