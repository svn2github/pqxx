/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/binarystring.hxx
 *
 *   DESCRIPTION
 *      declarations for bytea (binary string) conversions
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/binarystring instead.
 *
 * Copyright (c) 2003-2006, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler-public.hxx"

#include <string>

#include "pqxx/result"


namespace pqxx
{

/// Reveals "unescaped" version of PostgreSQL bytea string
/** @addtogroup escaping String escaping
 * This class represents a postgres-internal buffer containing the original,
 * binary string represented by a field of type bytea.  The raw value returned
 * by such a field contains escape sequences for certain characters, which are
 * filtered out by binarystring.
 *
 * The resulting string is zero-terminated, but may also contain zero bytes (or
 * indeed any other byte value) so don't assume that it can be treated as a
 * C-style string unless you've made sure of this yourself.
 *
 * The binarystring retains its value even if the result it was obtained from is
 * destroyed, but it cannot be copied or assigned.
 *
 * \relatesalso escape_binary
 *
 * To convert the other way, i.e. from a raw series of bytes to a string
 * suitable for inclusion as bytea values in your SQL, use the escape_binary()
 * functions.
 *
 * @warning This class is implemented as a reference-counting smart pointer.
 * Copying, swapping, and destroying binarystring objects that refer to the same
 * underlying data block is <em>not thread-safe</em>.  If you wish to pass
 * binarystrings around between threads, make sure that each of these operations
 * is protected against concurrency with similar operations on the same object,
 * or other objects pointing to the same data block.
 */
class PQXX_LIBEXPORT binarystring : internal::PQAlloc<unsigned char>
{
  // TODO: Templatize on character type?
public:
  typedef content_type char_type;
  typedef PGSTD::char_traits<char_type>::char_type value_type;
  typedef size_t size_type;
  typedef long difference_type;
  typedef const value_type &const_reference;
  typedef const value_type *const_pointer;
  typedef const_pointer const_iterator;

#ifdef PQXX_HAVE_REVERSE_ITERATOR
  typedef PGSTD::reverse_iterator<const_iterator> const_reverse_iterator;
#endif

private:
  typedef internal::PQAlloc<value_type> super;

public:
  /// Read and unescape bytea field
  /** The field will be zero-terminated, even if the original bytea field isn't.
   * @param F the field to read; must be a bytea field
   */
  explicit binarystring(const result::field &F); 			//[t62]

  /// Size of converted string in bytes
  size_type size() const throw () { return m_size; }			//[t62]
  /// Size of converted string in bytes
  size_type length() const throw () { return size(); }			//[t62]
  bool empty() const throw () { return size()==0; }			//[t62]

  const_iterator begin() const throw () { return data(); }		//[t62]
  const_iterator end() const throw () { return data()+m_size; }		//[t62]

  const_reference front() const throw () { return *begin(); }		//[t62]
  const_reference back() const throw () { return *(data()+m_size-1); }	//[t62]

#ifdef PQXX_HAVE_REVERSE_ITERATOR
  const_reverse_iterator rbegin() const 				//[t62]
  	{ return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const 					//[t62]
  	{ return const_reverse_iterator(begin()); }
#endif

  /// Unescaped field contents
  const value_type *data() const throw () {return super::c_ptr();}	//[t62]

  const_reference operator[](size_type i) const throw () 		//[t62]
  	{ return data()[i]; }

  bool operator==(const binarystring &) const throw ();			//[t62]
  bool operator!=(const binarystring &rhs) const throw ()		//[t62]
  	{ return !operator==(rhs); }

  /// Index contained string, checking for valid index
  const_reference at(size_type) const;					//[t62]

  /// Swap contents with other binarystring
  void swap(binarystring &);						//[t62]

  /// Raw character buffer (no terminating zero is added)
  /** @warning No terminating zero is added!  If the binary data did not end in
   * a null character, you will not find one here.
   */
  const char *c_ptr() const throw () 					//[t62]
  {
    return reinterpret_cast<char *>(super::c_ptr());
  }

  /// Read as regular C++ string (may include null characters)
  /** Caches string buffer to speed up repeated reads.
   *
   * @warning The first invocation of this function on a given binarystring
   * is not threadsafe; the first invocation constructs the string object and
   * stores it in the binarystring.  After it has been called once, any
   * subsequent calls on the same binarystring are safe.
   */
  const PGSTD::string &str() const;					//[t62]

private:
  size_type m_size;
  mutable PGSTD::string m_str;
};


/**
 * @addtogroup escaping String escaping
 */
//@{
/// Escape binary string for inclusion in SQL
/** \relatesalso binarystring
 */
PGSTD::string PQXX_LIBEXPORT escape_binary(const PGSTD::string &bin);
/// Escape binary string for inclusion in SQL
/** \relatesalso binarystring
 */
PGSTD::string PQXX_LIBEXPORT escape_binary(const char bin[]);
/// Escape binary string for inclusion in SQL
/** \relatesalso binarystring
 */
PGSTD::string PQXX_LIBEXPORT escape_binary(const char bin[], size_t len);
/// Escape binary string for inclusion in SQL
/** \relatesalso binarystring
 */
PGSTD::string PQXX_LIBEXPORT escape_binary(const unsigned char bin[]);
/// Escape binary string for inclusion in SQL
/** \relatesalso binarystring
 */
PGSTD::string PQXX_LIBEXPORT escape_binary(const unsigned char bin[], size_t len);
//@}


}

