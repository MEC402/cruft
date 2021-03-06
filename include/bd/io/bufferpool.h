//
// Created by jim on 4/10/16.
//

#ifndef bufferpool_h__
#define bufferpool_h__

#include <bd/log/logger.h>
#include <bd/io/buffer.h>

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace bd
{


///////////////////////////////////////////////////////////////////////////////
/// \brief Read blocks of data
///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
class BufferPool
{
public:


  ////////////////////////////////////////////////////////////////////////////////
  BufferPool(size_t bufSize, int numBuffers);


  ~BufferPool();


  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Allocate the memory for this BufferPool.
  ///////////////////////////////////////////////////////////////////////////////
  void allocate();


  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Wait for a full buffer to be placed in the queue and return it.
  ///////////////////////////////////////////////////////////////////////////////
  Buffer<Ty>* nextFull();


  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Return an empty buffer to the queue to be filled again.
  ///////////////////////////////////////////////////////////////////////////////
  void returnEmpty(Buffer<Ty>*);


  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Get an empty buffer from the queue.
  ///////////////////////////////////////////////////////////////////////////////
  Buffer<Ty>* nextEmpty();
  

  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Return a full buffer to the queue.
  ///////////////////////////////////////////////////////////////////////////////
  void returnFull(Buffer<Ty>*);


  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Return the size in elements of the Buffers.
  /// \note This may not be the actual length in elements of each Buffer, 
  ///       since the buffer may not have been entirely filled.
  ///////////////////////////////////////////////////////////////////////////////
  size_t bufferSizeElements() const;


  bool hasNext();
    

  void kickThreads();

  ///////////////////////////////////////////////////////////////////////////////
  /// \brief Return all buffers to empty pool.
  /// \note Function not thread safe.
  ///////////////////////////////////////////////////////////////////////////////
  void reset();

private:

  Ty *m_mem;
  std::vector<Buffer<Ty>*> m_allBuffers;
  std::queue<Buffer<Ty>*> m_emptyBuffers;
  std::queue<Buffer<Ty>*> m_fullBuffers;

  int m_nBufs;
  size_t m_szBytesTotal;

  std::mutex m_emptyBuffersLock;
  std::mutex m_fullBuffersLock;
  std::condition_variable_any m_emptyBuffersAvailable;
  std::condition_variable_any m_fullBuffersAvailable;

};



///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
BufferPool<Ty>::BufferPool(size_t bufSize, int nbuf)
  : m_mem{ nullptr }
  , m_nBufs{ nbuf }
  , m_szBytesTotal{ bufSize }
{ }


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
BufferPool<Ty>::~BufferPool()
{
  for( auto buf : m_allBuffers ) {
    delete buf;
  }

  if (m_mem) delete[] m_mem;
}

template<typename Ty>
void
BufferPool<Ty>::allocate()
{
  size_t buffer_size_elems{ bufferSizeElements() };

  m_mem = new Ty[ buffer_size_elems * m_nBufs ];
  Info() << "Allocated " << buffer_size_elems * m_nBufs << " elements ( " << 
      m_szBytesTotal << " bytes)."; 

  for(int i=0; i < m_nBufs; ++i) {
    size_t offset{ i * buffer_size_elems };
    Ty *start{ m_mem + offset };
    Buffer<Ty> *buf{ new Buffer<Ty>{start, buffer_size_elems} };
    m_allBuffers.push_back( buf );
    m_emptyBuffers.push( buf );
  }

  Info() << "Generated " << m_allBuffers.size() << " buffers of size " << 
      buffer_size_elems;

}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
Buffer<Ty> *
BufferPool<Ty>::nextFull()
{
  m_fullBuffersLock.lock();
  while(m_fullBuffers.size() == 0) {
    m_fullBuffersAvailable.wait(m_fullBuffersLock);
  }

  Buffer<Ty> *buf = m_fullBuffers.front();
  m_fullBuffers.pop();

  m_fullBuffersLock.unlock();

  return buf;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BufferPool<Ty>::returnEmpty(Buffer<Ty> *buf)
{
  // make sure this is one of our buffers.
//  auto thing = std::find(m_allBuffers.begin(), m_allBuffers.end(), buf);
//  if (thing == m_allBuffers.end()) {
//    Err() << "This was not one of our buffers!";
//    return;
//  }

  m_emptyBuffersLock.lock();

  m_emptyBuffers.push(buf);

  m_emptyBuffersLock.unlock();

  m_emptyBuffersAvailable.notify_all();
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
Buffer<Ty> *
BufferPool<Ty>::nextEmpty()
{
  m_emptyBuffersLock.lock();
  while(m_emptyBuffers.size() == 0) {
    m_emptyBuffersAvailable.wait(m_emptyBuffersLock);
  }

  Buffer<Ty> *buf{ m_emptyBuffers.front() };
  m_emptyBuffers.pop();

  m_emptyBuffersLock.unlock();

  return buf;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BufferPool<Ty>::returnFull(Buffer<Ty> *buf)
{
  m_fullBuffersLock.lock();
  m_fullBuffers.push(buf);
  m_fullBuffersLock.unlock();
  m_fullBuffersAvailable.notify_all();
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
size_t
BufferPool<Ty>::bufferSizeElements() const
{
  return (m_szBytesTotal / m_nBufs) / sizeof(Ty);
}

template<typename Ty>
bool
BufferPool<Ty>::hasNext()
{
  m_emptyBuffersLock.lock();
  if(m_emptyBuffers.size() == m_allBuffers.size()) {
    m_emptyBuffersLock.unlock();
    return false;
  }

  m_emptyBuffersLock.unlock();
  return true;
}

template<typename Ty>
void
BufferPool<Ty>::kickThreads()
{
    m_emptyBuffersAvailable.notify_all();
    m_fullBuffersAvailable.notify_all();
}

template<typename Ty>
void
BufferPool<Ty>::reset()
{
  for(Buffer<Ty> *b : m_allBuffers) {
    b->elements(bufferSizeElements());
    b->index(0);
    m_emptyBuffers.push(b);
  }
  while (! m_fullBuffers.empty()) {
    m_fullBuffers.pop();
  }
}

} // namespace preproc

#endif // ! bufferpool_h__ 
