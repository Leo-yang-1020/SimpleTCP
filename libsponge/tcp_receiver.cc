#include "tcp_receiver.hh"
#include "../tcp_helpers/tcp_segment.hh"
#include "../tcp_helpers/tcp_header.hh"
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template<typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    //! 静态变量表示绝对seq，便于记录上一个absolute seq

    static uint64_t absolute_seqno=0;

    if (header.syn && !syn_flag) {
        //该头为SYN，且为首次收到SYN
        _isn = header.seqno.raw_value();
        syn_flag = true;
        //初始化_ackno
        _ackno = _isn + 1;
    }
    if (syn_flag != true) {
        //尚未接收到syn报文，不能进行数据的接收
        return;
    }
    if(header.fin){
        fin_flag=true;
    }


    //!以下步骤为数据导入流重组器中

    //将数据域内数据转化为data
    string data = seg.payload().copy();

    //将报文的seqno转化为 absolute seqno,注意checkpoint通常设置为上一次的seq（静态变量表示）为基准
    absolute_seqno = unwrap(header.seqno,WrappingInt32(_isn),absolute_seqno);

    uint64_t stream_index=absolute_seqno;
    if(!header.syn){
        //如果是SYN报文，则absolute_seqno应该为0，不需要减去
        //stream index = absolute seqno-1(SYN占据一个seq，但是stream index中不占据)
        stream_index = absolute_seqno - 1;
    }

    //将数据导入到流重组器，关闭标志为FIN，只有存在FIN标志且当所有流均重组后才会关闭
    _reassembler.push_substring(data, stream_index, header.fin);

    //更新 absolute ackno ，由于SYN占据了一个seq,因此需要加1
    uint64_t absolute_ackno = _reassembler.get_ack() + 1;

    if (fin_flag && _reassembler.stream_out().input_ended()) {
        //如果有结束标记,我们需要判断该报文是否被完全接收，可以通过流重组器是否已经关闭输入来判断是否完全接收该FIN报文
        absolute_ackno ++;
    }
    // 将absolute ackno转化为seq
    _ackno = wrap(absolute_ackno, WrappingInt32(_isn)).raw_value();
}

optional <WrappingInt32> TCPReceiver::ackno() const {
    if (!syn_flag) {
        return nullopt;
        //等同于 return {};
    }
    //_ackno表示的是相对序号
    return WrappingInt32(_ackno);
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
bool TCPReceiver::syn_received() {
    return syn_flag;
}
