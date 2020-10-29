//////////////////////////////////////////
// Simple functional test
//
// With a serial loopback installed on the serial port,
// send and receive messages.
//
// Peter Fettterer (kb3gtn@gmail.com)
//
///////////////////////////////////////////


#include <iostream>
#include <string>

#include "serial_interface.hpp"

// user device to use.  (replace with COM<N> on windows)
#define serial_device "/dev/ttyUSB0"

using baud_rate = asio::serial_port::baud_rate;
using character_size = asio::serial_port::character_size;
using parity = asio::serial_port::parity;
using stop_bits = asio::serial_port::stop_bits;
using flow_control = asio::serial_port::flow_control;


void print_buffer(std::vector<uint8_t> buffer ) {
    std::ios_base::fmtflags f( std::cout.flags() );
    for (auto &b : buffer ) {
        if (std::isprint(b)) {
            std::cout << b;
        } else {
            std::cout << std::hex << "{0x" << (int)b << "}";
        }
    }
    std::cout.flags( f );
}


int main() {
    
    // make Thread Safe Queue shared pointer
    std::shared_ptr<ThreadSafeQueue<uint8_t>> rx_message_queue = std::make_shared<ThreadSafeQueue<uint8_t>>();

    // create a serial inteface instance
    SerialInterface::SerialInterface serial_port(
        rx_message_queue,
        std::string( serial_device ),
        baud_rate(9600),
        character_size(8),
        parity(parity::none),
        stop_bits(stop_bits::one),
        flow_control(flow_control::none)      
    );

    // create a serial tokenizer instance
    SerialInterface::SerialTokenizer token_parser(rx_message_queue);
    token_parser.set_token_delimiters(std::string(";"));
    token_parser.set_max_token_size(32);

    int loop_cnt = 0;

    std::string data_message;
    std::vector<uint8_t> buffer;

    std::cout << "Entering Main Loop.\n";

    while( loop_cnt < 100 ) {
        // send some serial messages
        data_message = "Hello World "+std::to_string(loop_cnt)+';';
        std::cout << "Sending: " << "'" << data_message << "'\n";
        serial_port.tx_data_sync(data_message);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        SerialInterface::SerialTokenizer::Result proc_result;

        if ( (proc_result = token_parser.poll_token(buffer)) == 
                SerialInterface::SerialTokenizer::Result::token_returned ) {
            std::cout << "token received: " << buffer.size() << "bytes : [ ";
            print_buffer( buffer );
            std::cout << " ]\n";
        } else {
            if ( proc_result == SerialInterface::SerialTokenizer::Result::token_length_error) {
                std::cout << "Token Length error, reset serial processor.\n";
            }
        }   
        ++loop_cnt;
    }

    SerialInterface::SerialTokenizer::Result proc_result;

    if ( (proc_result = token_parser.poll_token(buffer)) == 
            SerialInterface::SerialTokenizer::Result::token_returned ) {
        std::cout << "token received: " << buffer.size() << "bytes : [ ";
        print_buffer( buffer );
        std::cout << " ]\n";
    } else {
        if ( proc_result == SerialInterface::SerialTokenizer::Result::token_length_error) {
            std::cout << "Token Length error, reset serial processor.\n";
        }
    }   
 

    return 0;
}

