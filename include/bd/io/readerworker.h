#ifndef readerworker_h__
#define readerworker_h__

#include <bd/log/logger.h>
#include <bd/io/bufferpool.h>
#include <bd/io/bufferedreader.h>
#include <bd/io/buffer.h>


#include <fstream>
#include <atomic>
#include <string>

namespace bd
{


template<typename Reader, typename Pool, typename Ty>
class ReaderWorker
{
public:
  ReaderWorker(Reader *r, Pool *p)
    : m_reader{ r }
    , m_pool{ p }
    , m_is{ nullptr }
  { }

  ~ReaderWorker()
  {
    if (m_is) delete m_is;
  }

  long long
  operator()(const std::atomic_bool &quit)
  {
    if (! open()) {
        Err() << "RW: Could not open file " << m_path << 
            ". Exiting readerworker loop.";
        return -1;
    }

    // bytes to attempt to read from file.
    const size_t buffer_size_bytes{ m_pool->bufferSizeElements() * sizeof(Ty) };
    size_t total_read_bytes{ 0 };

    Info() << "RW: Entering loop"; 
    while(!(m_is->eof()) || !quit) {
      Info() << "RW: Waiting for buffer in readerworker loop.";

      Buffer<Ty> *buf = m_pool->nextEmpty();
      buf->index(total_read_bytes / sizeof(Ty));  // element index this buffer starts at.
      Ty *data = buf->ptr();
        
      Info() << "RW: Reading into buffer.";
      m_is->read(reinterpret_cast<char*>(data), buffer_size_bytes);
      std::streampos amount{ m_is->gcount() };
      Info() << "RW: Read " << amount << " bytes.";
      
      // the last buffer filled may not be a full buffer, so resize!
      if ( amount < static_cast<long long>(buffer_size_bytes) ) {

        buf->elements(amount/sizeof(Ty));
        if (amount == 0) {
          Info() << "RW: Returning empty buffer.";
          m_pool->returnEmpty(buf);
          Info() << "RW: Returned empty buffer.";
          break;
        }
      } // if (amount...)
        
      Info() << "RW: Going to return full buffer.";
      m_pool->returnFull(buf);
      Info() << "RW: Returned full buffer.";

      total_read_bytes += amount;
    } // while

    m_is->close();
    Info() << "RW: Leaving IO loop after reading " << total_read_bytes << " bytes";
    return total_read_bytes;
  }

    
  void
  setPath(const std::string &path)
  {
    m_path = path;
  }


private:
  bool
  open()
  {
    m_is = new std::ifstream();
    m_is->open(m_path, std::ios::binary);
    return m_is->is_open();
  }




  Reader *m_reader;
  Pool *m_pool;
  std::ifstream *m_is;
  std::string m_path;

}; // ReaderWorker


} //namespace preproc

#endif // ! readerworker_h__
