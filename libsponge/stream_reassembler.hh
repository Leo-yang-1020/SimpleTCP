#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"
#include <cstdint>
#include <string>
#include <set>
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:

// Your code here -- add private members as necessary.
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity=0;    //!< The maximum number of bytes
    size_t _ack=0;       //!<The excepted next index of bytes out of the ByteStream
    size_t _unassembled_bytes=0; //!<The number of the unassembled bytes (the bytes that not been read nor assembled in reassembler)
    bool _is_eof=false;   //!<The flag of whether is eof
    struct block_node{
        size_t begin=0;
        size_t len=0;
        std::string data="";
        bool operator<(const block_node t) const { return begin < t.begin;}
    };
    std::set<block_node> _blocks = {}; //!struct of the each block input but haven't been read yet



  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);
    StreamReassembler(const size_t capacity,ByteStream output);
    long merge_block(block_node &x,const block_node &y);
    void cut_block(size_t &begin,size_t &end,size_t index);
    void judge_eof(bool eof);
    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t get_ack();
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
