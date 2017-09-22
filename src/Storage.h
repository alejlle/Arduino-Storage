/**
 * @file Storage.h
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2017, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef STORAGE_H
#define STORAGE_H

/**
 * External memory storage interface, data block read/write and
 * streaming class. Handles linear allocation of storage blocks on the
 * device.
 */
class Storage {
public:
  /** Number of bytes on device. */
  const uint32_t SIZE;

  /**
   * Create storage manager with given number of bytes.
   * @param[in] size number of bytes on device.
   */
  Storage(uint32_t size) : SIZE(size), m_addr(0) {}

  /**
   * Returns number of bytes that may be allocated.
   * @return number of bytes.
   */
  uint32_t room()
  {
    return (SIZE - m_addr);
  }

  /**
   * Allocate block with given number of bytes on storage. Returns
   * storage address if successful otherwise UINT32_MAX.
   * @param[in] count number of bytes.
   * @return address of allocated block, otherwise UINT32_MAX.
   */
  uint32_t alloc(size_t count)
  {
    if (count > room()) return (UINT32_MAX);
    uint32_t res = m_addr;
    m_addr += count;
    return (res);
  }

  /**
   * Read count number of bytes from storage address to buffer.
   * @param[in] dest destination buffer pointer.
   * @param[in] src source memory address on device.
   * @param[in] count number of bytes to read from device.
   * @return number of bytes read or negative error code.
   */
  virtual int read(void* dest, uint32_t src, size_t count) = 0;

  /**
   * Write count number of bytes to storage address from buffer.
   * @param[in] dest destination memory address on device.
   * @param[in] src source buffer pointer.
   * @param[in] count number of bytes to write to device.
   * @return number of bytes written or negative error code.
   */
  virtual int write(uint32_t dest, const void* src, size_t count) = 0;

  /**
   * Storage Block for data; temporary or persistent external storage
   * of data.
   */
  class Block {
  public:
    /**
     * Construct block on given storage device with the given local
     * buffer, member size and number of members. Storage is allocated
     * on the device for the total storage of the members. The buffer
     * is assumed to only hold a single member. Default is a single
     * member.
     * @param[in] mem storage device for block.
     * @param[in] buf buffer address.
     * @param[in] size number of bytes per member.
     * @param[in] nmemb number of members (default 1).
     */
    Block(Storage &mem, void* buf, size_t size, size_t nmemb = 1) :
      SIZE(size),
      NMEMB(nmemb),
      m_mem(mem),
      m_addr(m_mem.alloc(size * nmemb)),
      m_buf(buf)
    {
    }

    /**
     * Returns storage total size (size * nmemb).
     * @return address.
     */
    uint32_t size()
    {
      if (NMEMB == 1)
	return (SIZE);
      return (SIZE * NMEMB);
    }

    /**
     * Returns storage address for the block.
     * @return address.
     */
    size_t addr()
    {
      return (m_addr);
    }

    /**
     * Read indexed storage block to buffer. Default index is the
     * first member.
     * @param[in] ix member index (default 0):
     * @return number of bytes read or negative error code.
     */
    int read(size_t ix = 0)
    {
      if (ix == 0)
	return (m_mem.read(m_buf, m_addr, SIZE));
      if (ix < NMEMB)
	return (m_mem.read(m_buf, m_addr + (ix * SIZE), SIZE));
      return (-1);
    }

    /**
     * Write buffer to indexed storage block. Default index is the
     * first member.
     * @param[in] ix member index (default 0):
     * @return number of bytes written or negative error code.
     */
    int write(size_t ix = 0)
    {
      if (ix == 0)
	return (m_mem.write(m_addr, m_buf, SIZE));
      if (ix < NMEMB)
	return (m_mem.write(m_addr + (ix * SIZE), m_buf, SIZE));
      return (-1);
    }

    /** Size of member. */
    const size_t SIZE;

    /** Number of members. */
    const size_t NMEMB;

  protected:
    /** Storage device from block. */
    Storage& m_mem;

    /** Address on storage device. */
    const uint32_t m_addr;

    /** Buffer for data. */
    void* m_buf;
  };

  /**
   * Stream of given size on given storage. Write/print intermediate
   * data to the stream that may later be read and transfered.
   * Multiple stream may be created on the same device by assigning
   * start address and size.
   */
  class Stream : public ::Stream {
  public:
    /**
     * Construct stream on given storage device with the given size.
     * address.
     * @param[in] mem storage device for stream.
     * @param[in] size number of bytes in stream.
     */
    Stream(Storage &mem, size_t size) :
      SIZE(size),
      m_mem(mem),
      m_addr(m_mem.alloc(size)),
      m_put(0),
      m_get(0),
      m_count(0)
    {
    }

    /**
     * Returns storage address.
     * @return address.
     */
    uint32_t addr()
    {
      return (m_addr);
    }

    /**
     * @override{Stream}
     * Write given byte to stream. Return number of bytes written,
     * zero if full.
     * @param[in] byte to write.
     * @return number of bytes written(1).
     */
    virtual size_t write(uint8_t byte)
    {
      if (m_count == SIZE) return (0);
      m_mem.write(m_addr + m_put, &byte, sizeof(byte));
      m_count += 1;
      m_put += 1;
      if (m_put == SIZE) m_put = 0;
      return (sizeof(byte));
    }

    /**
     * @override{Stream}
     * Write given buffer and numbe of bytes to stream. Return number
     * of bytes written.
     * @param[in] bufffer to write.
     * @param[in] size number of byets to write.
     * @return number of bytes.
     */
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
      uint16_t room = SIZE - m_count;
      if (room == 0) return (0);
      if (size > room) size = room;
      size_t res = size;
      room = SIZE - m_put;
      if (size > room) {
	m_mem.write(m_addr + m_put, buffer, room);
	buffer += room;
	size -= room;
	m_count += room;
	m_put = 0;
      }
      m_mem.write(m_addr + m_put, buffer, size);
      m_count += size;
      m_put += size;
      return (res);
    }

    /**
     * @override{Stream}
     * Returns number of bytes available for read().
     * @return bytes available.
     */
    virtual int available()
    {
      return (m_count);
    }

    /**
     * @override{Stream}
     * Return next byte to read if available otherwise negative error
     * code(-1).
     * @return next byte or negative error code.
     */
    virtual int peek()
    {
      if (m_count == 0) return (-1);
      uint8_t res = 0;
      m_mem.read(&res, m_addr + m_get, sizeof(res));
      return (res);
    }

    /**
     * @override{Stream}
     * Return next byte if available otherwise negative error
     * code(-1).
     * @return next byte or negative error code.
     */
    virtual int read()
    {
      if (m_count == 0) return (-1);
      uint8_t res = 0;
      m_mem.read(&res, m_addr + m_get, sizeof(res));
      m_count -= 1;
      m_get += 1;
      if (m_get == SIZE) m_get = 0;
      return (res);
    }

    /**
     * @override{Stream}
     * Flush all data and reset stream.
     */
    virtual void flush()
    {
      m_put = 0;
      m_get = 0;
      m_count = 0;
    }

    /** Total size of the stream. */
    size_t SIZE;

  protected:
    /** Storage device for the stream. */
    Storage& m_mem;

    /** Address on storage for the stream data. */
    const uint32_t m_addr;

    /** Index for the next write. */
    uint16_t m_put;

    /** Index for the next read. */
    uint16_t m_get;

    /** Number of bytes available. */
    uint16_t m_count;
  };

protected:
  /** Address of the next alloc. */
  uint32_t m_addr;
};
#endif