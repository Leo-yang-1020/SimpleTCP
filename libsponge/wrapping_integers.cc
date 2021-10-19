#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template<typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32(static_cast<uint32_t>(n) + isn.raw_value());
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t high_32_bit = checkpoint & 0xFFFFFFFF00000000;
    //记录checkpoint的高32位，用于确定区间
    //假设当前为第一个4GB的数据
    uint32_t num;
    if (n.raw_value() >= isn.raw_value()) {
        num = n - isn;
    } else {
        //已经发生了正溢出
        num = (UINT32_MAX - isn.raw_value()) + n.raw_value()+1;
    }
    uint64_t absolute_seqno = high_32_bit + num;
    //当前有三种情形
    //！< absolute seqno已经是最接近checkpoint
    // !< absolute seqno需要再加2^32才最接近checkpoint
    // !< absolute seqno需要再减去2^32才最接近checkpoint :这里有一个坑，必须要保证absolute seqno大于1<<32的减法才有效
    // !< 反例：isn=0 seqno=2^32-1 checkpoint=0 此时很明显应该是absolute seqno= seqno，但是做减法可以减到负数！
    if (abs(int64_t (absolute_seqno-checkpoint)) > abs(int64_t (absolute_seqno + (1ul << 32)-checkpoint))){
        //判断是否加上2^32更接近checkpoint
        absolute_seqno = absolute_seqno + (1ul<<32);
    }else if ((absolute_seqno>=(1ul<<32))&&abs(int64_t (absolute_seqno-checkpoint)) > abs(int64_t (absolute_seqno - (1ul << 32)-checkpoint))) {
        absolute_seqno = absolute_seqno - (1ul<<32);
    }
    return absolute_seqno;

}
//int main(){
//    uint64_t res=unwrap(WrappingInt32(UINT32_MAX), WrappingInt32(0), 0);
//    printf("%lu",res);
//}




