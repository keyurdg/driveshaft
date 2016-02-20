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

#ifndef incl_DRIVESHAFT_STATUS_LOOP_H_
#define incl_DRIVESHAFT_STATUS_LOOP_H_

#include <boost/asio.hpp>
#include <snyder/metrics_registry.h>
#include "thread-registry.h"

namespace Driveshaft {

  extern Snyder::MetricsRegistry* MetricsRegistry;

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
