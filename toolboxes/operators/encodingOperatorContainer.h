/** \file encodingOperatorContainer.h
    \brief An encoding operator that can contain multiple other encoding operators. Use it when more than one encoding operator is required in a solver.
*/

#pragma once

#include "linearOperator.h"

#include <iostream>
#include <vector>
#include <boost/smart_ptr.hpp>
#include <sstream>
#include <stdexcept>

namespace Gadgetron{

  template <class ARRAY_TYPE> class encodingOperatorContainer : public linearOperator<ARRAY_TYPE>
  {
  public:

    encodingOperatorContainer() : linearOperator<ARRAY_TYPE>() { num_elements_ = 0; }
    virtual ~encodingOperatorContainer(){}

    // The domain and codomain dimensions of this container cannot be set. 
    // They should be set indirectly through the contained operators instead.
    //
    virtual void set_domain_dimensions( std::vector<unsigned int>* ){
      BOOST_THROW_EXCEPTION(runtime_error( "Error: encodingOperatorContainer::set_domain_dimensions() : operation not supported." ));
    }
    
    virtual void set_codomain_dimensions( std::vector<unsigned int>* ){
      BOOST_THROW_EXCEPTION(runtime_error( "Error: encodingOperatorContainer::set_codomain_dimensions() : operation not supported." ));
    }
    
    // Get domain and codomain dimensions:
    // The domain should match between the individual operators.
    // The codomain is a concatenation of the indivudial operators' domains.
    //
    virtual boost::shared_ptr< std::vector<unsigned int> > get_domain_dimensions() 
    { 
      if( operators_.size() == 0 ){
	BOOST_THROW_EXCEPTION(runtime_error( "Error: encodingOperatorContainer::get_domain_dimensions() : no operators present." ));
      }
      
      boost::shared_ptr< std::vector<unsigned int> > dims = (operators_[0])->get_domain_dimensions();
      for( unsigned int i=1; i<operators_.size(); i++ )
	if( *dims != *((operators_[i])->get_domain_dimensions()) ){
	  BOOST_THROW_EXCEPTION(runtime_error( "Error: encodingOperatorContainer::get_domain_dimensions() : inconsistent operator dimensions." ));
	}
      return dims;
    }
    
    virtual boost::shared_ptr< std::vector<unsigned int> > get_codomain_dimensions() 
    { 
      if( num_elements_ == 0 ){
	BOOST_THROW_EXCEPTION(runtime_error( "Error: encodingOperatorContainer::get_codomain_dimensions() : no operators present." ));
      }
      
      std::vector<unsigned int> *dims = new std::vector<unsigned int>();
      dims->push_back(num_elements_);
      return boost::shared_ptr< std::vector<unsigned int> >(dims);
    }

    // Get domain and codomain for the individual operators
    //
    virtual boost::shared_ptr< std::vector<unsigned int> > get_domain_dimensions(unsigned int i) 
    { 
      if( i>=operators_.size() )
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::get_domain_dimensions : illegal index provided"));
      return operators_[i]->get_domain_dimensions(); 
    }
  
    virtual boost::shared_ptr< std::vector<unsigned int> > get_codomain_dimensions(unsigned int i) 
    { 
      if( i>=operators_.size() )
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::get_codomain_dimensions : illegal index provided"));
      return operators_[i]->get_codomain_dimensions(); 
    }
  
    // Allocate an array of the codomain dimensions
    //
    boost::shared_ptr< ARRAY_TYPE> create_codomain() 
    {
      return boost::shared_ptr<ARRAY_TYPE>(new ARRAY_TYPE(get_codomain_dimensions()));
    }
  
    // Concatenate a vector of codomains into a single array
    //
    boost::shared_ptr< ARRAY_TYPE> create_codomain( std::vector<ARRAY_TYPE*> codoms )
    {
      if (codoms.size() != operators_.size())
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainter::create_codomain: number of operators and number of codomains do no match"));

      boost::shared_ptr<ARRAY_TYPE> codomain(new ARRAY_TYPE(get_codomain_dimensions()));
      unsigned int offset = 0;

      for (unsigned int i = 0; i < operators_.size(); i++){

	if (!codoms[i]->dimensions_equal(get_codomain_dimensions(i).get())){
	  std::stringstream ss;
	  ss << "encodingOperatorContainter::create_codomain: input codomain " << i << " does not match corresponding operator codomain" << std::endl;
	  ss << "Input codomain: ";
	  std::vector<unsigned int> ico = *codoms[i]->get_dimensions();
	  for (unsigned int k = 0; k < ico.size(); k++) ss << ico[k] << " ";
	  ss << std::endl;
	  ss << "Operator codomain: ";
	  ico = *get_codomain_dimensions(i);
	  std::cout << "SIZE: " << ico.size() << std::endl;
	  for (unsigned int k = 0; k < ico.size(); k++) ss << ico[k] << " ";
	  ss << std::endl;
	  BOOST_THROW_EXCEPTION(runtime_error(ss.str()));
	}

	ARRAY_TYPE slice;
	slice.create(codoms[i]->get_dimensions().get(),codomain->get_data_ptr()+offset);
	slice = *codoms[i];
	offset += slice.get_number_of_elements();
      }

      return codomain;    
    }

    // Get individual operators
    //
    boost::shared_ptr< linearOperator<ARRAY_TYPE> > get_operator(unsigned int i)
    {
      if( i>=operators_.size() )
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::get_operator : illegal index provided"));
      return operators_[i];
    }

    // Get pointer offset into codomain for individual operators "sub-codomains"
    //
    unsigned int get_offset(unsigned int i)
    {
      if( i>=operators_.size() )
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::get_offset : illegal index provided"));
      return offsets_[i];
    }
  
    // Add operator to the container
    //
    void add_operator( boost::shared_ptr< linearOperator<ARRAY_TYPE> > op )
    {
      boost::shared_ptr< std::vector<unsigned int> > codomain = op->get_codomain_dimensions();
      
      if( codomain->size() == 0 ){
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::add_operator : codomain dimensions not set on operator"));
      }

      unsigned int elements = 1;
      for (unsigned int i=0; i<codomain->size(); i++){
	elements *= codomain->at(i);
      }
    
      if( elements == 0 ){
	BOOST_THROW_EXCEPTION(runtime_error("encodingOperatorContainer::add_operator : illegal codomain dimensions on operator"));
      }

      if (offsets_.size() == 0){
	offsets_.push_back(0);
      } else{
	offsets_.push_back(num_elements_);
      }

      num_elements_ += elements;
      operators_.push_back(op);
    }
  
    virtual void mult_M( ARRAY_TYPE* in, ARRAY_TYPE* out, bool accumulate = false )
    {
      for (unsigned int i=0; i<operators_.size(); i++){
	ARRAY_TYPE tmp_data(operators_[i]->get_codomain_dimensions(),out->get_data_ptr()+offsets_[i]);
	operators_[i]->mult_M( in, &tmp_data, accumulate );
      }
    }

    virtual void mult_MH( ARRAY_TYPE* in, ARRAY_TYPE* out, bool accumulate = false )
    {
      ARRAY_TYPE tmp_image(get_domain_dimensions());
        
      for (unsigned int i=0; i<operators_.size(); i++){
      
	boost::shared_ptr< linearOperator<ARRAY_TYPE> > op = operators_[i];
	ARRAY_TYPE tmp_data(op->get_codomain_dimensions(),in->get_data_ptr()+offsets_[i]);
      
	// This operator is special in that it needs to apply the "internal" operator weights
	//

	op->mult_MH( &tmp_data, &tmp_image );

	if( i == 0 && !accumulate ){
	  *out = tmp_image;
	  *out *= op->get_weight();
	}
	else {
	  axpy( op->get_weight(), &tmp_image, out );
	}
      }
    }
  
    virtual void mult_MH_M( ARRAY_TYPE* in, ARRAY_TYPE* out, bool accumulate = false )
    {

      ARRAY_TYPE tmp_image(get_domain_dimensions());
    
      for (unsigned int i=0; i<operators_.size(); i++){
      
	boost::shared_ptr< linearOperator<ARRAY_TYPE> > op = operators_[i];
      
	// This operator is special in that it needs to apply the "internal" operator weights
	//
      
	op->mult_MH_M( in, &tmp_image );
	if( i == 0 && !accumulate ){
	  *out = tmp_image;
	  *out *= op->get_weight();
	}
	else {
	  axpy( op->get_weight(), &tmp_image, out ) ;
	}
      }
    }

    virtual boost::shared_ptr< linearOperator< ARRAY_TYPE> > clone()
    {
      return linearOperator< ARRAY_TYPE >::clone(this);
    }
  
  protected:
    std::vector< boost::shared_ptr< linearOperator<ARRAY_TYPE> > > operators_;
    std::vector<unsigned int> offsets_;
    unsigned int num_elements_;
  };
}
