//////////////////////////////////////////////////////////////
// ASIO Serial Interface
//
// Provides a ASIO context for a serial port receive data
//
///////////////////////////////////////////////////////////

#ifndef __serial_interface_hpp__
#define __serial_interface_hpp__

#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <cctype>
#include <algorithm>
#include <functional>

// include asio library
#include "asio.hpp"
#include "ThreadSafeQueue.hpp"

namespace SerialInterface {

// maps asio types needed here..
using baud_rate = asio::serial_port::baud_rate;
using character_size = asio::serial_port::character_size;
using parity = asio::serial_port::parity;
using stop_bits = asio::serial_port::stop_bits;
using flow_control = asio::serial_port::flow_control;

class SerialInterface {
    public:
       enum class Result {
            ok,
            failed
        };
        SerialInterface(
            std::shared_ptr<ThreadSafeQueue<uint8_t>> rx_queue,
            std::string serial_port,    // serial port string
            baud_rate br,               // baudrate
            character_size,             // data bits
            parity p,                   // parity
            stop_bits sb,               // stop bits
            flow_control fc             // flow control signaling
        ) : m_rx_queue(rx_queue) {
            // zero out rx byte holding var..
            m_byte_buffer = 0;
            // create a unique instance of an ASIO io_service
            io = std::make_shared<asio::io_service>();
            // create serial driver instance and assign it to an io_service instance.
            sp = std::make_shared<asio::serial_port>(*io);
            sp->open(serial_port); 
            // prime the asio read pump
            asio::system_error error;
            size_t bytes_rx=0;
            sp->async_read_some( asio::buffer(&m_byte_buffer, 1), std::bind(&SerialInterface::on_rx_data,this,error,bytes_rx));
            // start service loop thread
            service_thread = std::thread( [this] { this->io->run(); });
        }

        ~SerialInterface() {
            // signal io context to shutdown
            io->stop();
            // wait for service_thread to exit..
            service_thread.join();
        }

        // blocking send for vector buffers
        Result tx_data_sync( std::vector<uint8_t> buffer ) {
            sp->write_some( asio::buffer(buffer));
            return Result::ok;
        }

        // blocking send for strings buffers
        Result tx_data_sync( std::string buffer ) {
            sp->write_some( asio::buffer(buffer));
            return Result::ok;
        }

    private:

        // callback from async_read_some after read was completed.
        // push rx byte onto rx_queue, kick off another read.
        Result on_rx_data(const asio::system_error &error, size_t& bytes_rx) {
            // push rx byte onto output queue.
            m_rx_queue->push(m_byte_buffer);
            // kick off another asio read request.
            sp->async_read_some( asio::buffer(&m_byte_buffer, 1), std::bind(&SerialInterface::on_rx_data,this,error,bytes_rx));
            return Result::ok;
        }

        uint8_t m_byte_buffer;
        std::shared_ptr<asio::io_service> io;
        std::shared_ptr<asio::serial_port> sp;
        std::thread service_thread;
        // output queue for async received bytes to be stored in as they arrive.
        std::shared_ptr<ThreadSafeQueue<uint8_t>> m_rx_queue;
};

// read rx queue and tokenize input stream
class SerialTokenizer {
    public:
        enum class Result {
            token_returned,
            no_token_available,
            token_length_error
        };
        // constuctor
        SerialTokenizer(std::shared_ptr<ThreadSafeQueue<uint8_t>> rx_queue ) : m_rx_queue(rx_queue) {
            // default max token size
            m_max_token_size = 128;
            // default working buffer initialization
            m_working_buffer.clear();
            // size working buffer to hold max token size.
            m_working_buffer.reserve(m_max_token_size);
            // setup default delimiters
            m_delimiters = { ' ' };
        }

        // set delimters for tokens
        void set_token_delimiters(std::string delimters ) {
            m_delimiters.clear();
            std::copy( delimters.begin(), delimters.end(), std::back_inserter(m_delimiters) );
        }

        // set max token size from default
        void set_max_token_size(size_t size) {
            m_max_token_size = size;
            m_working_buffer.clear();
            m_working_buffer.reserve(m_max_token_size);
        }

        // return next token if there is one detected in rx_queue.
        Result poll_token( std::vector<uint8_t> &token_out ) {
            token_out.clear();
            uint8_t byte;
            // see if there is a byte in the rx_queue
            while( m_rx_queue->try_pop(byte) ) {
                // push byte onto working buffer
                m_working_buffer.push_back(byte);
                ++byte_cnt;
                // check max token length
                if ( byte_cnt >= m_max_token_size ) {
                    // reset processor
                    m_working_buffer.clear();
                    // reset byte count in current token
                    byte_cnt = 0;
                    // return that we has a token length error.
                    return Result::token_length_error; 
                }
                // test input to see if it matches a delimiter byte
                auto it = std::find(m_delimiters.begin(), m_delimiters.end(), byte);

                // check to see if received byte is a delimiter
                if (it != m_delimiters.end() ) {
                    // It is a delimiter..
                    // copy working buffer contents into provided buffer
                    std::copy( m_working_buffer.begin(), m_working_buffer.end(), std::back_inserter(token_out));
                    // reset working buffer for next token
                    m_working_buffer.clear();
                    byte_cnt = 0;
                    return Result::token_returned;
                }
            }
            // end of rx qeueue, wait for next poll to get rest of token data.
            return Result::no_token_available;
        }

    private:
        size_t byte_cnt;
        size_t m_max_token_size;
        std::vector<uint8_t> m_delimiters;
        std::vector<uint8_t> m_working_buffer;
        std::shared_ptr<ThreadSafeQueue<uint8_t>> m_rx_queue;
};


} // end namespace

#endif // end include guard

