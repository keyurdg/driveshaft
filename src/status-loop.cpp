/*
 * Driveshaft: Gearman worker manager
 *
 * Copyright (c) [2015] [Keyur Govande]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <sstream>
#include <memory>
#include "status-loop.h"

namespace Driveshaft {

/*
 * A quick primer on boost asio follow. It is all callbacks.
 * m_acceptor sets up a listener on the port, or throws an exception if it cannot
 * bind. The 'true' param at the end enables SO_REUSE.
 *
 * async_accept takes in a socket and a callback to call whenever accept
 * succeeds. async_accept returns immediately and transfers control to
 * io_service.run() in the calling context. This happens for all the following
 * callbacks too.
 *
 * handleAccept is called when there's a new connection. m_socket is filled-in
 * with the newest connection. Create a StatusResponder shared_ptr to handle
 * the actual IO. This sets up a callback for async_read_until which looks for
 * a "\n". If/when it finds one or EOF, it calls the callback to handleRead.
 * Note how we're passing in self as the bound object instead of "this". This
 * creates a copy of shared_ptr<StatusResponder> and hence keeps the object
 * alive for the callback.
 *
 * handleRead gets the data off the wire and makes a response and calls
 * async_write with a handleWrite callback. async_write will ensure the full
 * buffer passed to it is written on the socket. It may make multiple calls
 * to the handleWrite callback. On completing the full flush, the
 * shared_ptr<StatusResponder> aka "self" reaches ref-count == 0 and is
 * destructed.
 */

using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace std::placeholders;

static const std::string CR("\r");
static const std::string STATUS_COMMAND_THREADS("threads");
static const std::string STATUS_COMMAND_THREADS_CR(STATUS_COMMAND_THREADS + CR);
static const std::string STATUS_COMMAND_COUNTERS = "counters";
static const std::string STATUS_COMMAND_COUNTERS_CR(STATUS_COMMAND_COUNTERS + CR);
static const std::string STATUS_COMMAND_GAUGES("gauges");
static const std::string STATUS_COMMAND_GAUGES_CR(STATUS_COMMAND_GAUGES + CR);

class StatusResponder : public std::enable_shared_from_this<StatusResponder> {
private:
    tcp::socket m_socket;
    ThreadRegistryPtr m_registry;
    boost::asio::streambuf m_read_buf;
    boost::asio::streambuf m_write_buf;

    void handleWrite(const boost::system::error_code& error, size_t bytes) noexcept {
        LOG4CXX_DEBUG(StatusLogger, "Transmitted " << bytes << " status bytes. Error: " << error);
    }

    void handleRead(const boost::system::error_code& error, std::size_t size) noexcept {
        if (error) {
            // This is normal if for example the client goes away without sending a command
            LOG4CXX_DEBUG(StatusLogger, "StatusResponder::handleRead got an error " << error);
        } else {
            auto self(shared_from_this());
            std::ostream os(&m_write_buf);
            std::string input;

            std::istream is(&m_read_buf);
            std::getline(is, input);

            LOG4CXX_DEBUG(StatusLogger, "Read string " << input << " from socket");

            if (!input.compare(STATUS_COMMAND_THREADS)
                || !input.compare(STATUS_COMMAND_THREADS_CR)) {
                LOG4CXX_DEBUG(StatusLogger, "Sending thread-map back");

                const auto thread_map = m_registry->getThreadMap();

                for (const auto& i : thread_map) {
                    const auto &data = i.second;
                    os << i.first << "\t" << data.pool << "\t" << data.should_shutdown << "\t" << data.state << "\r\n";
                }
            } else if (!input.compare(STATUS_COMMAND_COUNTERS)
                || !input.compare(STATUS_COMMAND_COUNTERS_CR)) {

              LOG4CXX_DEBUG(StatusLogger, "Getting counters snapshot and sending it back.");
              auto counters = Driveshaft::MetricsRegistry->GetCounters();
              for (auto& kv : counters) {
                    os << kv.first << ": " << kv.second << std::endl;
              }
            } else if (!input.compare(STATUS_COMMAND_GAUGES)
                || !input.compare(STATUS_COMMAND_GAUGES_CR)) {

              LOG4CXX_DEBUG(StatusLogger, "Getting gauges snapshot and sending it back.");
              auto gauges = Driveshaft::MetricsRegistry->GetGauges();
              for (auto& kv : gauges) {
                    os << kv.first << ": " << kv.second << std::endl;
              }
            } else {
                os << "Error: unrecognized command\r\n";
            }

            boost::asio::async_write(m_socket, m_write_buf, std::bind(&StatusResponder::handleWrite, self, _1, _2));
        }
    }

    StatusResponder() = delete;
    StatusResponder(const StatusResponder&) = delete;
    StatusResponder(StatusResponder&&) = delete;
    StatusResponder& operator=(const StatusResponder&) = delete;
    StatusResponder& operator=(const StatusResponder&&) = delete;

    StatusResponder(tcp::socket&& sock, ThreadRegistryPtr reg) noexcept
    : m_socket(std::move(sock))
    , m_registry(reg)
    , m_read_buf()
    , m_write_buf() {
        LOG4CXX_DEBUG(StatusLogger, "Started StatusResponder");
    }

public:
    /*
     * This class should only be constructed as a shared_ptr object so
     * std::enable_shared_from_this() works. A raw new is wrong. Hence this
     * static method to enforce this rule.
     * At any place in this class, if you need a shared_ptr to "this" only
     * use shared_from_this(). Don't wrap "this" in a shared_ptr by hand.
     *
     */
    static std::shared_ptr<StatusResponder> makeNew(tcp::socket&& sock, ThreadRegistryPtr reg) noexcept {
        return std::shared_ptr<StatusResponder>(new StatusResponder(std::move(sock), reg));
    }

    ~StatusResponder() = default;

    void run() noexcept {
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
        LOG4CXX_DEBUG(StatusLogger, "StatusLoop::handleAccept got an error: " << error);
    } else {
        LOG4CXX_DEBUG(StatusLogger, "Accepted status connection without error");
        auto responder = StatusResponder::makeNew(std::move(m_socket), m_registry);
        responder->run();
    }

    startAccept();
}

}
