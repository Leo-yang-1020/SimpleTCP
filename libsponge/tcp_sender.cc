#include "tcp_sender.hh"

#include <optional>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initiail_retransmission_tmeout{retx_timeout}
    , _rto(_initiail_retransmission_tmeout)
    , _stream(capacity) {}

/**
 * 返回已经发送但还没有确认的报文的数量
 * @return
 */
uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }
/**
 * fill_window函数需要完成sender业务逻辑，包括SYN，FIN报文，从ByteStream中读取数据并发送等等的逻辑,其逻辑关联需要用到FSM
 * 实现逻辑如下：
 *     *尚未发送SYN：
 *          *发送SYN
 *     *已经发送了FIN：
 *           *直接返回
 *     *已经发送了SYN：
 *           *还没有收到ACK:
 *                 *直接返回
 *           *ByteStream为空：
 *                 *字节流已经关闭：发送FIN
 *                 *字节流尚未关闭：直接返回
 *           *ByteStream不为空：
 *                 *我们尽可能填充更多的payload(到最大值为止)
 *                 填充后：
 *                 *字节流已经关闭 ->
 *                          *字节流关闭输入（主动关闭）说明需要发送FIN报文，则报文中需要带上FIN首部（前提是有剩余空间）
 *                 *字节流没有关闭
 *                          *如果_stream中仍然有内容并且_receiver_free_space也有空间->继续填充
 *      *窗口值为0：
 *            为了避免sender与receiver双方陷入死锁，需要有零窗口探测机制：
 *                 *当窗口值为0且没有发送过窗口时：将窗口视作1并发送一个探测窗口，避免receiver的窗口不为0后陷入死锁
 *
 *  fill_window()函数不考虑重发的情况
 */
void TCPSender::fill_window() {
    //填充window的原则为尽可能一个segment足够大，不超过MSS的最大值，也不超过window size
    if (!_syn_sent) {
        // SYN尚未发送，发送端必须先发送SYN报文，且SYN报文通常不携带数据
        _syn_sent = true;
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg);
        return;
    }
    if (!_outstanding_segment.empty() && _outstanding_segment.front().header().syn)
        // SYN已经发送但未确认
        return;
    if (!_stream.buffer_size() && !_stream.eof())
        //字节流为空且没有关闭
        return;
    if (_fin_sent)
        // FIN
        return;

    if (_window_size == 0 && !_zero_window_sent) {
        TCPSegment seg;
        //零窗口探测机制
        if (!_stream.buffer_empty() && !_stream.eof()) {
            seg.payload() = _stream.read(1);
        } else if (_stream.eof()) {
            seg.header().fin = true;
        }
        _zero_window_sent = true;
        send_segment(seg);
        return;
    }
    //
    while (_receiver_free_space > 0) {
        TCPSegment seg;
        if (_stream.buffer_empty()) {
            if (_stream.eof() && !_fin_sent && _receiver_free_space > 0) {
                seg.header().fin = true;
                _fin_sent = true;
                send_segment(seg);
            }
            break;
        }

        //新的报文的payload的大小的限制:_receiver_free_space,流的大小，max_payload_size，三者取最小值
        size_t payload_size =
            min(min((_stream.buffer_size()), static_cast<size_t>(_receiver_free_space)), (TCPConfig::MAX_PAYLOAD_SIZE));
        seg.payload() = _stream.read(payload_size);

        if (_stream.eof() && payload_size < _receiver_free_space) {
            //当流已经关闭并且有剩余空间->将报文设置FIN标志（也会占据一个size）
            seg.header().fin = true;
            _fin_sent = true;
        }
        send_segment(seg);
    }
}

void TCPSender::send_segment(TCPSegment &segment) {
    if (!_timer_running) {
        //发送报文需要开启timer
        _timer_running = true;
    }

    segment.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += segment.length_in_sequence_space();
    _bytes_in_flight += segment.length_in_sequence_space();

    if (!segment.header().syn && _receiver_free_space >= segment.length_in_sequence_space()) {
        _receiver_free_space -= segment.length_in_sequence_space();
    }

    _segments_out.push(segment);
    _outstanding_segment.push(segment);
}

/**
 * 根据传递来的新的ackno(窗口的左边界)以及window_size
 * 需要计算receiver_free_space,_next_seqno
 * 注意：ackno以及窗口大小并不一定能够更新 这里只处理
 * 窗口更新后，需要对存储outgoing segment的数据结构也进行更新(这里使用队列存储)
 */
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_seg_ack = unwrap(ackno, _isn, _next_seqno);
    if (!is_valid_ackno(absolute_seg_ack, window_size)) {
        //不符合规范的报文直接return
        return;
    }
    _window_size = window_size;

    while (!_outstanding_segment.empty()) {
        //由于接受端采取的是累计确认的机制，因此收到的ack及之前的所有报文都已经到达，需要将其移出outgoing队列

        TCPSegment un_ack_seg = _outstanding_segment.front();
        uint64_t outgoing_seg_right_bound =
            un_ack_seg.length_in_sequence_space() + unwrap(un_ack_seg.header().seqno, _isn, absolute_seg_ack) - 1;
        if (absolute_seg_ack > outgoing_seg_right_bound) {
            //只有某个segment被完整地确认后才能出队
            _outstanding_segment.pop();
            _rto = _initiail_retransmission_tmeout;
            _time_passed = 0;
            _consecutive_retransmissions = 0;
            _bytes_in_flight -= un_ack_seg.length_in_sequence_space();
            _zero_window_sent = false;
        } else {
            break;
        }
    }

    if (_outstanding_segment.empty()) {
        //当outgoing队列清空时，关闭定时器
        _timer_running = false;
    }

    //一个ACK报文到达后，需要更新_receiver_free_space，sender才能正确地发送指定数量的报文
    //!表示该ack报文请求的最右的seq
    uint64_t absolute_right_bound = absolute_seg_ack + static_cast<uint64_t>(window_size) - 1;
    if (_next_seqno <= absolute_right_bound) {
        //如果该报文的最右seq尚未发送，需要填满窗口

        //接下会调用fill_window()函数，决定了fill_window()函数将会发送多大的报文
        _receiver_free_space = (absolute_right_bound - _next_seqno + 1);
    }
    fill_window();
}
/**
 * 接收到的ACK可能无效的情况有：
 *   *ACK冗余->该ACK表示的报文已经被确认过了
 *   *ACK范围问题：
 *          *出现了absolute ackno==0的情况
 *          *出现了absolute ackno超出了发送的报文的情况
 * @param ackno
 * @return
 */
bool TCPSender::is_valid_ackno(uint64_t absolute_seg_ack, uint16_t window_size) {
    //
    bool ack_value = false;

    if (!_outstanding_segment.empty()) {
        //当_outstanding_segment不为空时，检查该报文是否具有确认价值->能够确认某些已发送的报文
        TCPSegment first_unack_seg = _outstanding_segment.front();
        uint64_t unack_seg =unwrap(first_unack_seg.header().seqno, _isn, _next_seqno);
        ack_value = absolute_seg_ack > unack_seg;
    }
    uint64_t new_seg_right = absolute_seg_ack + static_cast<uint64_t>(window_size) - 1;
    //判断某一个报文是否具有让发送端发送新报文的价值->其右值大于等于_next_seqno
    bool send_value = _next_seqno <= new_seg_right;
    //报文满足基本的范围要求，并且具有发送价值或者确认价值，那么这个报文就是有意义的
    return absolute_seg_ack > 0 && absolute_seg_ack <= _next_seqno && (ack_value || send_value);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running) {
        return;
    }
    if (ms_since_last_tick + _time_passed >= _rto) {
        //已经超时
        TCPSegment seg = _outstanding_segment.front();
        _segments_out.push(seg);  //相当于重发最早未确认的报文
        if (_window_size != 0) {
            //窗口不为0时，将_rto的值翻倍，连续重发次数++
            _rto <<= 1;
            _consecutive_retransmissions++;
        }
        _time_passed = 0;
        _zero_window_sent = false;  //超时后又可以发送零窗口探测报文
    } else {
        _time_passed += ms_since_last_tick;
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

