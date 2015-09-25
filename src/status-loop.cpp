#include <sstream>
#include <boost/bind.hpp>
#include "status-loop.h"

namespace Driveshaft {

using namespace boost::asio;
using boost::asio::ip::tcp;

StatusLoop::StatusLoop(io_service& io_service, ThreadRegistryPtr reg)
    : m_acceptor(io_service, tcp::endpoint(tcp::v4(), STATUS_PORT), true)
    , m_deadline_timer(io_service)
    , m_registry(reg) {
    LOG4CXX_DEBUG(StatusLogger, "Starting status loop");
    setDeadline();
    startAccept();
}

void StatusLoop::setDeadline() noexcept {
    if (g_force_shutdown) {
        LOG4CXX_INFO(StatusLogger, "Shutdown requested. Status thread turning off");
        m_deadline_timer.get_io_service().stop();
        return;
    }

    LOG4CXX_DEBUG(StatusLogger, "Setting deadline of " << LOOP_SLEEP_DURATION << " seconds");
    m_deadline_timer.expires_from_now(boost::posix_time::seconds(LOOP_SLEEP_DURATION));
    m_deadline_timer.async_wait(boost::bind(&StatusLoop::handleDeadline, this, boost::asio::placeholders::error));
}

void StatusLoop::handleDeadline(const boost::system::error_code& error) noexcept {
    if (error) {
        LOG4CXX_ERROR(StatusLogger, "StatusLoop::handleDeadline got an error: " << error);
    } else {
        LOG4CXX_DEBUG(StatusLogger, "Deadline expired without errors");
    }

    setDeadline();
}

void StatusLoop::startAccept() noexcept {
    if (g_force_shutdown) {
        LOG4CXX_INFO(StatusLogger, "Shutdown requested. Status thread turning off");
        m_acceptor.get_io_service().stop();
        return;
    }

    LOG4CXX_DEBUG(StatusLogger, "Setting async_accept in StatusLoop");
    auto socket = std::make_shared<tcp::socket>(m_acceptor.get_io_service());
    m_acceptor.async_accept(*socket, boost::bind(&StatusLoop::handleAccept, this, socket, boost::asio::placeholders::error));
}

void StatusLoop::handleAccept(tcp_socket_ptr socket_ptr, const boost::system::error_code& error) noexcept {
    if (!error) {
        LOG4CXX_DEBUG(StatusLogger, "Accepted status connection without error");
        const auto thread_map = m_registry->getThreadMap();
        std::stringstream stream;

        for (const auto& i : thread_map) {
            const auto &data = i.second;
            stream << i.first << "\t" << data.pool << "\t" << data.should_shutdown << "\t" << data.state << std::endl;
        }

        boost::asio::async_write(*socket_ptr, boost::asio::buffer(stream.str()),
                                 boost::bind(&StatusLoop::handleWrite, this, socket_ptr, boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));

    } else {
        LOG4CXX_ERROR(StatusLogger, "StatusLoop::handleAccept got an error: " << error);
    }

    startAccept();
}

void StatusLoop::handleWrite(tcp_socket_ptr socket_ptr, const boost::system::error_code& error, size_t bytes) noexcept {
    if (error) {
        LOG4CXX_ERROR(StatusLogger, "StatusLoop::handleWrite got an error: " << error);
    } else {
        LOG4CXX_DEBUG(StatusLogger, "Transmitted " << bytes << " status bytes ");
    }
}
}
