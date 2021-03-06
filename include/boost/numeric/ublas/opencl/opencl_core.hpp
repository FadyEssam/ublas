#ifndef OPENCL_MATRIX
#define OPENCL_MATRIX

#include <boost/compute/core.hpp>
#include <boost/compute/algorithm.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/functional.hpp>
#include <boost/compute/buffer.hpp>
#include <boost/compute/container/vector.hpp>
#include <clBLAS.h>

namespace boost {
namespace numeric {
namespace ublas {

namespace opencl
{
  namespace compute = boost::compute;
  namespace ublas = boost::numeric::ublas;

  /// a class that is used as a tag to tell that the matrix resides on a device
  class storage;


  ///a class to initialize and finalize library
  class library
  {

  public:

	///start clBlas (opencl backend library)
	library()
	{
	  clblasSetup();
	}

	/// Finalize clBLAS (opencl backend library)
	~library()
	{
	  clblasTeardown();
	}

  };




}//opencl

  /** \brief A dense matrix of values of type \c T that resides on a specific device using opencl.
  *
  * this is a special case of class boost::numeric::ublas::matrix<T, L, A> that has A as opencl::storage
  * so it is stored on a specific device and stores some information about this device.
  *
  *
  * For a \f$(m \times n)\f$-dimensional matrix and \f$ 0 \leq i < m, 0 \leq j < n\f$, every element \f$ m_{i,j} \f$ is mapped to
  * the \f$(i.n + j)\f$-th element of the container for row major orientation or the \f$ (i + j.m) \f$-th element of
  * the container for column major orientation.
  *
  *
  * \tparam T the type of object stored in the matrix (like double, float, complex, etc...)
  * \tparam L the storage organization. It can be either \c row_major or \c column_major. Default is \c row_major
  */
template<class T, class L>
class matrix<T, L, opencl::storage> : public matrix_container<matrix<T, L, opencl::storage> >

{
  typedef typename boost::compute::buffer_allocator<T>::size_type size_type;
  typedef L layout_type;
  typedef matrix<T, L, opencl::storage> self_type;

public:

  // Construction

  /** Dense matrix constructor with size (0,0)
  */
  matrix()
	: matrix_container<self_type>(),
	size1_(0), size2_(0), data_() , device_() , context_() {}


  /** Dense matrix constructor with size (size1,size2) and resides on memory of device of context c
  * \param size1 number of rows
  * \param size2 number of columns
  * \param context is the context that the matrix will be stored on
  */
  matrix(size_type size1, size_type size2, compute::context c)
	: matrix_container<self_type>(),
	size1_(size1), size2_(size2), device_(c.get_device()), context_(c)
  {
	compute::buffer_allocator<T> allocator(c);
	data_ = allocator.allocate(layout_type::storage_size(size1, size2)).get_buffer();
  }



  /** Dense matrix constructor with size (size1,size2) and resides on memory of device of queue q and initialize all elements to value
  * \param size1 number of rows
  * \param size2 number of columns
  * \param value is the value that all elements of the matrix are set to
  * \param q is the command queue of the device which will store the matrix and do the filling
  */
  matrix(size_type size1, size_type size2, const T& value, compute::command_queue &q)
	: matrix_container<self_type>(),
	size1_(size1), size2_(size2), device_(q.get_device()), context_(q.get_context())
  {
	compute::buffer_allocator<T> allocator(q.get_context());
	data_ = allocator.allocate(layout_type::storage_size(size1, size2)).get_buffer();

	compute::fill(this->begin(), this->end(), value, q);

	q.finish();
  }

  /**matrix constructor that copies a matrix on host to device of the queue
  * \param m is the matrix to be copied to device
  * \param queue is the command queue which its device will execute the copying and will have the new matrix
  */
  template<class A>
  matrix(matrix<T, L, A>& m, compute::command_queue& queue) : matrix(m.size1(), m.size2(), queue.get_context())
  {
	this->from_host(m, queue);
  }

  //frees the allocated buffer
  ~matrix()
  {
	size_t size = layout_type::storage_size(size1_, size2_);

	if (size > 0)
	{
	  compute::buffer_allocator<T> allocator(context_);
	  allocator.deallocate(data_, size);
	}

  }

  // Accessors
  /** Return the number of rows of the matrix
  */
  size_type size1() const {
	return size1_;
  }

  /** Return the number of colums of the matrix
  */
  size_type size2() const {
	return size2_;
  }

  /** Return the compute::buffer that has the begin data on the device
  */
  const compute::buffer_iterator<T> begin() const { compute::make_buffer_iterator<T>(data_); }
  compute::buffer_iterator<T> begin() { return compute::make_buffer_iterator<T>(data_); }


  /** Return the compute::buffer that has the end of the data on the device
  */
  compute::buffer_iterator<T> end() { return compute::make_buffer_iterator<T>(data_, layout_type::storage_size(size1_, size2_)); }
  const compute::buffer_iterator<T> end() const { return compute::make_buffer_iterator<T>(data_, layout_type::storage_size(size1_, size2_)); }

  /** Return boost::numeri::ublas::opencl::device that has informaton
  * about the device that the matrix resides on.
  */
  const compute::device &device() const { return device_; }
  compute::device &device() { return device_; }

  /**Fill all elements of the matrix with the value
  * \param value value to set all elements of the matrix to
  * \param queue is the command queue that will execute the operation
  */
  void fill(T value, compute::command_queue & queue)
  {
	assert(device_ == queue.get_device());

	compute::fill(this->begin(), this->end(), value, queue);

	queue.finish();
  }

  /** Copies a matrix to a device
  * \param m is a matrix that is not on same device and it is copied to it
  * \param queue is the command queue that will execute the operation
  */
  template<class A>
  void from_host(ublas::matrix<T, L, A>& m, compute::command_queue & queue)
  {
	assert(device_ == queue.get_device());

	assert((size2_ == m.size2()) && (size1_ == m.size1()));

	compute::copy(
	  m.data().begin(),
	  m.data().end(),
	  this->begin(),
	  queue
	);

	queue.finish();
  }


  /** Copies a matrix from a device
  * \param m is a matrix that the values of (*this) will be copied in it
  * \param queue is the command queue that will execute the operation
  */
  template<class A>
  void to_host(ublas::matrix<T, L, A>& m , compute::command_queue& queue)
  {
	assert(device_ == queue.get_device());

	assert((size2_ == m.size2()) && (size1_ == m.size1()));

	compute::copy(
	  this->begin(),
	  this->end(),
	  m.data().begin(),
	  queue
	);

	queue.finish();
  }


  ///swaps content between two matrices on opencl device
  void swap(ublas::matrix<T, L, opencl::storage>& m, compute::command_queue & queue)
  {
	assert( (this->size1() == m.size1()) && (this->size2() == m.size2()) );

	assert((this->device() == queue.get_device()) && (m.device() == queue.get_device()));

	compute::swap_ranges(this->begin(), this->end(), m.begin(), queue);

	queue.finish();
  }


private:
  size_type size1_;
  size_type size2_;
  compute::buffer data_;
  compute::device device_;
  compute::context context_;
};





/** \brief A vector of values of type \c T that resides on a specific device using opencl.
*
* this is a special case of class boost::numeric::ublas::vector<T, A> that has A as opencl::storage
* so it is stored on a specific device and stores some information about this device.
*
*
* \tparam T the type of object stored in the matrix (like double, float, complex, etc...)
*/
template <class T>
class vector<T, opencl::storage> : public boost::compute::vector<T>
{
  typedef std::size_t size_type;

public:

  // Construction


  /**vector constructor with size 0 on default context
  */
  vector() : compute::vector<T>() {}

  /**vector constructor with size (size) on a specific context
  * \param size is the size that will be initially allocated for the vector
  * \param context is the context that the data will be stored on
  */
  vector(size_type size, compute::context context) : compute::vector<T>(size, context) { device_ = context.get_device(); }

  /**vector constructor with size (size) on a specific context and initialize it to a value
  * \param size is the size that will be initially allocated for the vector
  * \param value is the initial value for all the vector lements
  * \param queue is the command queue that the data will be stored on it context and which will execute the operation
  */
  vector(size_type size,T value, compute::command_queue queue) : compute::vector<T>(size,value,  queue) {
	queue.finish();
	device_ = queue.get_device();
  }

  /**vector constructor that copies a vector on host to device of the queue
  * \param v is the vector to be copied to device
  * \param queue is the command queue which its device will execute the copying and will have the new vector on its device
  */
  template<class A>
  vector(vector<T, A>& v, compute::command_queue& queue) : vector(v.size(), queue.get_context())
  {
	this->from_host(v, queue);
  }


  /** Return boost::numeri::ublas::opencl::device that has informaton
  * about the device that the vector resides on.
  */
  const compute::device device() const { return device_; }
  compute::device device() { return device_; }



  /** Copies a vector to a device
  * \param v is a vector that is not on the same device and it is copied to it
  * \param queue is the command queue that will execute the operation
  */
  template<class A>
  void from_host(ublas::vector<T, A>& v, compute::command_queue & queue)
  {
	assert(this->device() == queue.get_device());

	assert(this->size() == v.size());

	compute::copy(
	  v.begin(),
	  v.end(),
	  this->begin(),
	  queue
	);

	queue.finish();
  }


  /** Copies a vector from a device
  * \param v is a vector that will be reized to (size_) and the values of (*this) will be copied in it
  * \param queue is the command queue that will execute the operation
  */
  template<class A>
  void to_host(ublas::vector<T, A>& v, compute::command_queue& queue)
  {
	assert(this->device() == queue.get_device());

	assert(this->size() == v.size());

	compute::copy(
	  this->begin(),
	  this->end(),
	  v.begin(),
	  queue
	);

	queue.finish();

  }

  /**Fill all elements of the vector with the value
  * \param value is the value to set all elements of the vector to
  * \param queue is the command queue that will execute the operation
  */
  void fill(T value, compute::command_queue & queue)
  {
	assert(this->device() == queue.get_device());

	compute::fill(this->begin(), this->end(), value, queue);
	
	queue.finish();
  }

  ///swaps content between two vectors on opencl device
  void swap(ublas::vector<T, opencl::storage>& v, compute::command_queue & queue)
  {
	assert(this->size() == v.size());

	assert( (this->device() == queue.get_device()) && (v.device()==queue.get_device()) );

	compute::swap_ranges(this->begin(), this->end(), v.begin(), queue);

	queue.finish();
  }


private:
  compute::device device_;
};

}//ublas
}//numeric
}//boost





#endif 