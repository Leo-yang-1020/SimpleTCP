#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _ms_since_last_receive; }
/**
 *
 * @param seg
 */
void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_is_active) {
        return;
    }
    _ms_since_last_receive = 0;
    //收到的报文为rst的动作
    if (seg.header().rst) {
        unclean_shutdown(false);
        return;
    }
    if (is_listen()) {
        // receiver处于监听状态
        if (seg.header().syn == false) {
            // Listen状态下丢弃所有非SYN标志报文
            return;
        }
        if (is_syn_sent()) {
            if (seg.payload().size()) {
                // sender处于syn_sent receiver处于listen时，报文中不能带有数据（防止SYN洪泛)
                return;
            }
            //            if (!seg.header().ack) {
            //                // simultaneous open->同时打开，两个peer均为主动开启者
            //            }
        } else if (is_closed()) {
            //peer为被动连接方，需要发送带有ack的syn报文
            passive_connect(seg);
            return;
        }
    }
    _receiver.segment_received(seg);

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (seg.length_in_sequence_space() && _sender.stream_in().buffer_empty()) {
        _sender.send_empty_segment();
    }
    send_segments();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t written_size = _sender.stream_in().write(data);
    if (written_size > 0) {
        _sender.fill_window();
    }
    return written_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
//! 由于tick()会被周期性调用，因此所有超时，超限的处理都必须在tick()函数完成
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    //完成可能出现的超时重传
    _ms_since_last_receive += ms_since_last_tick;
    //由于_sender在执行tick后可能出现报文超时需要重传，报文会挂载到sender的队列
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        unclean_shutdown(true);
    }
    send_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segments();
}

void TCPConnection::passive_connect(TCPSegment segment) {
    _receiver.segment_received(segment);
    _sender.fill_window();
    send_segments();
}

/**
 * 将sender处挂载的报文转移到发送队列中
 *     *注意sender的报文并不是最终的报文，报文在sender处只是正确加载了 seqno,fin,syn标志位
 *     *还需要加载由receiver计算的 ackno,window_size等
 *
 */
void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            segment.header().ackno = _receiver.ackno().value();
            segment.header().ack = true;
        }
        segment.header().win = _receiver.window_size();

        if (_rst_flag) {
            segment.header().rst = true;
        }
        _segments_out.push(segment);
    }
    clean_shutdown();
}

void TCPConnection::clean_shutdown() {
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        // passive close不需要等待10rto
        _linger_after_streams_finish = false;
    }
    if (_receiver.stream_out().input_ended() && bytes_in_flight() == 0 && _receiver.unassembled_bytes() == 0 && _sender.stream_in().eof()) {
        if (!_linger_after_streams_finish || _ms_since_last_receive >= _cfg.rt_timeout * 10) {
            _is_active = false;
        }
    }
}

void TCPConnection::unclean_shutdown(bool send_rst) {
    _is_active = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _rst_flag = true;
    if (send_rst) {
        //主动发送RST
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        send_segments();
    }
}
bool TCPConnection::is_syn_sent() {
    return (_sender.next_seqno_absolute() > 0) && (_sender.next_seqno_absolute() == _sender.bytes_in_flight());
}

bool TCPConnection::is_closed() { return _sender.next_seqno_absolute() == 0; }

bool TCPConnection::is_listen() { return !_receiver.ackno().has_value(); }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            unclean_shutdown(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
