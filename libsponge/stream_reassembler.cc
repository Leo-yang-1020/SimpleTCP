#include <iostream>
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template<typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(ByteStream(capacity)), _capacity(capacity) {}

StreamReassembler::StreamReassembler(const size_t capacity, ByteStream output) : _output(output), _capacity(capacity) {}



/**
 * 合并两个碎片，返回合并时overlap的长度，如果碎片没有交集不能合并，则返回-1
 * 如果合并成功，合并后的碎片会放置在x，y碎片将失去意义
 * @param x
 * @param y
 */
long StreamReassembler::merge_block(block_node &x, const block_node &y) {
    block_node pre, next;
    if (x.begin < y.begin) {
        pre = x;
        next = y;
    } else {
        pre = y;
        next = x;
    }
    if (pre.begin + pre.len < next.begin) {
        return -1;
        //碎片不连续，无法合并
    } else if (pre.begin + pre.len >= next.begin + next.len) {
        //一碎片完全覆盖另一碎片
        x = pre;
        return next.len;
    } else {
        //一般可能有overlap的情况
        x.begin = pre.begin;
        x.data = pre.data + next.data.substr(pre.begin + pre.len - next.begin, next.len);
        x.len = x.data.length();
        return pre.begin + pre.len - next.begin;
    }
}

void StreamReassembler::cut_block(size_t &begin, size_t &end, size_t index) {
    if (_ack > index) {
        begin += (_ack - index);
    }
    size_t sr_max_index = _ack - _output.buffer_size() + _capacity-1;
    size_t block_max_index = index + end;
    if (block_max_index > sr_max_index) {
        end -= (block_max_index - sr_max_index);
    }
}

void StreamReassembler::judge_eof(bool eof) {
    if (eof) {
        _is_eof = true;
    }
    if (_is_eof && empty()) {
        _output.end_input();
        //当标记为该流为最后且所有流均已经重组后，可以关闭流的输入
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index + data.length() <= _ack) {
        //当新发送的碎片已经被完全读取
        judge_eof(eof);
        return;
    }
    if (index >= _ack - _output.buffer_size() + _capacity) {
        //当发送的新碎片已经完全超过最大容量
        return;
    }
    size_t begin = 0, end = data.size() - 1;

    cut_block(begin, end, index);

    if(begin>end){
        return;
    }

    block_node _elm;

    _elm.begin = index + begin;
    _elm.len = end - begin + 1;
    _elm.data = data.substr(begin, end + 1);

    auto itr = _blocks.lower_bound(_elm);
    //找到第一个大于等于begin的双向迭代器

    //merge next blocks
    while (itr != _blocks.end() && merge_block(_elm, *itr) != -1) {
        _blocks.erase(*itr);
        _unassembled_bytes -= (*itr).len;
        itr = _blocks.lower_bound(_elm);
    }
    if (itr == _blocks.begin()) {
        //在合并完所有后续碎片后发现插入的碎片是index最小的一个碎片
        goto ASAP;
    }
    //merge pre blocks
    //类似于OS中的紧凑算法，在紧凑前序节点时，只需要找到一个碎片即可。(因为最多合并到前一个碎片，能合并早就合并了)
    itr--;
    while (merge_block(_elm, *itr) >= 0) {
        _unassembled_bytes -= (*itr).len;
        _blocks.erase(itr);
        itr = _blocks.lower_bound(_elm);
        if (itr == _blocks.begin()) {
            break;
        }
        itr--;
    }
    ASAP:
    _blocks.insert(_elm);
    //send to ByteStream ASAP
    _unassembled_bytes += _elm.len;
    itr = _blocks.begin();
    if ((*itr).begin == _ack) {
        //首个碎片的index恰好等于_ack
        _output.write((*itr).data);
        _ack += (*itr).data.length();
        _unassembled_bytes -= (*itr).data.length();
        _blocks.erase(itr);
    }
    if(end==data.length()-1){
        judge_eof(eof);
    }

}


size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_bytes;
}

bool StreamReassembler::empty() const {
    return _unassembled_bytes == 0;
}
size_t StreamReassembler::get_ack() {
    return _ack;
}
//int main(){
//    StreamReassembler sr(1);
//    sr.push_substring("ab",0,false);
//    sr.push_substring("ab",0,false);
//    string data=sr.stream_out().read(1);
//    cout<<data;
//    sr.push_substring("abc",0,false);
//    data=sr.stream_out().read(1);
//    cout<<data;
//
//}
