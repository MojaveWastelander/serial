/* Copyright 2012 William Woodall and John Harrison */
#include <alloca.h>

#include "serial/serial.h"

#ifdef _WIN32
#include "serial/impl/win.h"
#else
#include "serial/impl/unix.h"
#endif

using std::invalid_argument;
using std::min;
using std::numeric_limits;
using std::vector;
using std::size_t;
using std::string;

using serial::Serial;
using serial::SerialExecption;
using serial::IOException;
using serial::bytesize_t;
using serial::parity_t;
using serial::stopbits_t;
using serial::flowcontrol_t;

class Serial::ScopedReadLock {
public:
  ScopedReadLock(SerialImpl *pimpl) : pimpl_(pimpl) {
    this->pimpl_->readLock();
  }
  ~ScopedReadLock() {
    this->pimpl_->readUnlock();
  }
private:
  SerialImpl *pimpl_;
};

class Serial::ScopedWriteLock {
public:
  ScopedWriteLock(SerialImpl *pimpl) : pimpl_(pimpl) {
    this->pimpl_->writeLock();
  }
  ~ScopedWriteLock() {
    this->pimpl_->writeUnlock();
  }
private:
  SerialImpl *pimpl_;
};

Serial::Serial (const string &port, unsigned long baudrate, long timeout,
                bytesize_t bytesize, parity_t parity, stopbits_t stopbits,
                flowcontrol_t flowcontrol)
 : read_cache_("")
{
  pimpl_ = new SerialImpl (port, baudrate, timeout, bytesize, parity,
                           stopbits, flowcontrol);
}

Serial::~Serial ()
{
  delete pimpl_;
}

void
Serial::open ()
{
  pimpl_->open ();
}

void
Serial::close ()
{
  pimpl_->close ();
}

bool
Serial::isOpen () const
{
  return pimpl_->isOpen ();
}

size_t
Serial::available ()
{
  return pimpl_->available ();
}

size_t
Serial::read_ (unsigned char *buffer, size_t size)
{
  return this->pimpl_->read (buffer, size);
}

size_t
Serial::read (unsigned char *buffer, size_t size)
{
  ScopedReadLock (this->pimpl_);
  return this->pimpl_->read (buffer, size);
}

size_t
Serial::read (std::vector<unsigned char> &buffer, size_t size)
{
  ScopedReadLock (this->pimpl_);
  unsigned char *buffer_ = new unsigned char[size];
  size_t bytes_read = this->pimpl_->read (buffer_, size);
  buffer.insert (buffer.end (), buffer_, buffer_+bytes_read);
  delete[] buffer_;
  return bytes_read;
}

size_t
Serial::read (std::string &buffer, size_t size)
{
  ScopedReadLock (this->pimpl_);
  unsigned char *buffer_ = new unsigned char[size];
  size_t bytes_read = this->pimpl_->read (buffer_, size);
  buffer.append (reinterpret_cast<const char*>(buffer_), bytes_read);
  delete[] buffer_;
  return bytes_read;
}

string
Serial::read (size_t size)
{
  std::string buffer;
  this->read (buffer, size);
  return buffer;
}

size_t
Serial::readline (string &buffer, size_t size, string eol)
{
  ScopedReadLock (this->pimpl_);
  size_t eol_len = eol.length ();
  unsigned char *buffer_ = static_cast<unsigned char*>
                              (alloca (size * sizeof (unsigned char)));
  size_t read_so_far = 0;
  while (true)
  {
    size_t bytes_read = this->read_ (buffer_ + read_so_far, 1);
    read_so_far += bytes_read;
    if (bytes_read == 0) {
      break; // Timeout occured on reading 1 byte
    }
    if (string (reinterpret_cast<const char*>
         (buffer_ + read_so_far - eol_len), eol_len) == eol) {
      break; // EOL found
    }
    if (read_so_far == size) {
      break; // Reached the maximum read length
    }
  }
  buffer.append(reinterpret_cast<const char*> (buffer_), read_so_far);
  return read_so_far;
}

string
Serial::readline (size_t size, string eol)
{
  std::string buffer;
  this->readline (buffer, size, eol);
  return buffer;
}

vector<string>
Serial::readlines (size_t size, string eol)
{
  ScopedReadLock (this->pimpl_);
  std::vector<std::string> lines;
  size_t eol_len = eol.length ();
  unsigned char *buffer_ = static_cast<unsigned char*>
    (alloca (size * sizeof (unsigned char)));
  size_t read_so_far = 0;
  size_t start_of_line = 0;
  while (read_so_far < size) {
    size_t bytes_read = this->read_ (buffer_+read_so_far, 1);
    read_so_far += bytes_read;
    if (bytes_read == 0) {
      if (start_of_line != read_so_far) {
        lines.push_back (
          string (reinterpret_cast<const char*> (buffer_ + start_of_line),
            read_so_far - start_of_line));
      }
      break; // Timeout occured on reading 1 byte
    }
    if (string (reinterpret_cast<const char*>
         (buffer_ + read_so_far - eol_len), eol_len) == eol) {
      // EOL found
      lines.push_back(
        string(reinterpret_cast<const char*> (buffer_ + start_of_line),
          read_so_far - start_of_line));
      start_of_line = read_so_far;
    }
    if (read_so_far == size) {
      if (start_of_line != read_so_far) {
        lines.push_back(
          string(reinterpret_cast<const char*> (buffer_ + start_of_line),
            read_so_far - start_of_line));
      }
      break; // Reached the maximum read length
    }
  }
  return lines;
}

size_t
Serial::write (const string &data)
{
  ScopedWriteLock(this->pimpl_);
  return pimpl_->write (data);
}

void
Serial::setPort (const string &port)
{
  ScopedReadLock(this->pimpl_);
  ScopedWriteLock(this->pimpl_);
  bool was_open = pimpl_->isOpen ();
  if (was_open) close();
  pimpl_->setPort (port);
  if (was_open) open ();
}

string
Serial::getPort () const
{
  return pimpl_->getPort ();
}

void
Serial::setTimeout (long timeout)
{
  pimpl_->setTimeout (timeout);
}

long
Serial::getTimeout () const {
  return pimpl_->getTimeout ();
}

void
Serial::setBaudrate (unsigned long baudrate)
{
  pimpl_->setBaudrate (baudrate);
}

unsigned long
Serial::getBaudrate () const
{
  return pimpl_->getBaudrate ();
}

void
Serial::setBytesize (bytesize_t bytesize)
{
  pimpl_->setBytesize (bytesize);
}

bytesize_t
Serial::getBytesize () const
{
  return pimpl_->getBytesize ();
}

void
Serial::setParity (parity_t parity)
{
  pimpl_->setParity (parity);
}

parity_t
Serial::getParity () const
{
  return pimpl_->getParity ();
}

void
Serial::setStopbits (stopbits_t stopbits)
{
  pimpl_->setStopbits (stopbits);
}

stopbits_t
Serial::getStopbits () const
{
  return pimpl_->getStopbits ();
}

void
Serial::setFlowcontrol (flowcontrol_t flowcontrol)
{
  pimpl_->setFlowcontrol (flowcontrol);
}

flowcontrol_t
Serial::getFlowcontrol () const
{
  return pimpl_->getFlowcontrol ();
}

void Serial::flush ()
{
  ScopedReadLock(this->pimpl_);
  ScopedWriteLock(this->pimpl_);
  pimpl_->flush ();
  read_cache_.clear ();
}

void Serial::flushInput ()
{
  ScopedReadLock(this->pimpl_);
  pimpl_->flushInput ();
}

void Serial::flushOutput ()
{
  ScopedWriteLock(this->pimpl_);
  pimpl_->flushOutput ();
  read_cache_.clear ();
}

void Serial::sendBreak (int duration)
{
  pimpl_->sendBreak (duration);
}

void Serial::setBreak (bool level)
{
  pimpl_->setBreak (level);
}

void Serial::setRTS (bool level)
{
  pimpl_->setRTS (level);
}

void Serial::setDTR (bool level)
{
  pimpl_->setDTR (level);
}

bool Serial::getCTS ()
{
  return pimpl_->getCTS ();
}

bool Serial::getDSR ()
{
  return pimpl_->getDSR ();
}

bool Serial::getRI ()
{
  return pimpl_->getRI ();
}

bool Serial::getCD ()
{
  return pimpl_->getCD ();
}