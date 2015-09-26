#include <sstream>
#include <memory>
#include "status-loop.h"

namespace Driveshaft {

using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace std::placeholders;

static const char* STATUS_COMMAND_THREADS = "threads";
static const char* STATUS_COMMAND_COUNTERS = "counters";
static const char* STATUS_COMMAND_GAUGES = "gauges";

class StatusResponder : public std::enable_shared_from_this<StatusResponder> {
private:
    tcp::socket m_socket;
    ThreadRegistryPtr m_registry;
    boost::asio::streambuf m_read_buf;
    boost::asio::streambuf m_write_buf;

    void handleWrite(const boost::system::error_code& error, size_t bytes) noexcept {
        if (error) {
            LOG4CXX_ERROR(StatusLogger, "StatusResponder::handleWrite got an error: " << error);
        } else {
            LOG4CXX_DEBUG(StatusLogger, "Transmitted " << bytes << " status bytes ");
        }
    }

    void handleRead(const boost::system::error_code& error, std::size_t size) {
        if (error) {
            LOG4CXX_ERROR(StatusLogger, "StatusResponder::handleRead got an error " << error);
        } else {
            LOG4CXX_DEBUG(StatusLogger, "Read " << size << " bytes from socket");

            auto self(shared_from_this());
            std::istream is(&m_read_buf);
            std::string input;
            std::getline(is, input);

            LOG4CXX_DEBUG(StatusLogger, "Read string " << input << " from socket");

            if (!input.compare(STATUS_COMMAND_THREADS)) {
                LOG4CXX_DEBUG(StatusLogger, "Sending thread-map back");

                const auto thread_map = m_registry->getThreadMap();
                std::ostream stream(&m_write_buf);

                for (const auto& i : thread_map) {
                    const auto &data = i.second;
                    stream << i.first << "\t" << data.pool << "\t" << data.should_shutdown << "\t" << data.state << "\n";
                }

                boost::asio::async_write(m_socket, m_write_buf, std::bind(&StatusResponder::handleWrite, self, _1, _2));
            }
        }
    }

public:
    StatusResponder(tcp::socket sock, ThreadRegistryPtr reg)
    : m_socket(std::move(sock))
    , m_registry(reg)
    , m_read_buf()
    , m_write_buf() {
        LOG4CXX_DEBUG(StatusLogger, "Started StatusResponder");
    }

    void run() {
        auto self(shared_from_this());
        boost::asio::async_read_until(m_socket, m_read_buf, "\n",
                                      std::bind(&StatusResponder::handleRead, self, _1, _2));
    }


};

StatusLoop::StatusLoop(io_service& io_service, ThreadRegistryPtr reg)
    : m_acceptor(io_service, tcp::endpoint(tcp::v4(), STATUS_PORT), true)
    , m_deadline_timer(io_service)
    , m_socket(io_service)
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
    m_deadline_timer.async_wait(std::bind(&StatusLoop::handleDeadline, this, _1));
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
    m_acceptor.async_accept(m_socket, std::bind(&StatusLoop::handleAccept, this, _1));
}

void StatusLoop::handleAccept(const boost::system::error_code& error) noexcept {
    if (error) {
        LOG4CXX_ERROR(StatusLogger, "StatusLoop::handleAccept got an error: " << error);
    } else {
        LOG4CXX_DEBUG(StatusLogger, "Accepted status connection without error");
        std::make_shared<StatusResponder>(std::move(m_socket), m_registry)->run();
    }

    startAccept();
}

}
