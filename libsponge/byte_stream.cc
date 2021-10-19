#include <iostream>
#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template<typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity) {}
size_t ByteStream::write(const string &data) {
    int max_write_size =min(remaining_capacity(),data.size());
    for(int i=0;i<max_write_size;i++){
        _deque.push_back(data[i]);
    }
    _total_written+=max_write_size;
    return max_write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length=min(len,_deque.size());
    return string().assign(_deque.begin(),_deque.begin()+length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    for (size_t i = 0; i < len && !_deque.empty(); i++) {
        _deque.pop_front();
        _total_read++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string str="";
    size_t i=0;
    for (i = 0; i < len && !_deque.empty(); i++) {
        str.append(1,_deque.front());
        _deque.pop_front();
    }
    _total_read+=i;
    return str;
}

void ByteStream::end_input() {
    flag_input_ended=true;
}

bool ByteStream::input_ended() const {
    return flag_input_ended;
}

size_t ByteStream::buffer_size() const {
    return _deque.size();
}

bool ByteStream::buffer_empty() const {
    return _deque.empty();
}

bool ByteStream::eof() const {
    return  _deque.empty()&&input_ended();
}

size_t ByteStream::bytes_written() const {
    return _total_written;
}

size_t ByteStream::bytes_read() const {
    return _total_read;
}

size_t ByteStream::remaining_capacity() const {
    return _capacity - _deque.size();
}

