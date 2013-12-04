// Filename: IBFEPostProcessor.h
// Created on 4 Dec 2013 by Boyce Griffith
//
// Copyright (c) 2002-2013, Boyce Griffith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of New York University nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef included_IBFEPostProcessor
#define included_IBFEPostProcessor

/////////////////////////////// INCLUDES /////////////////////////////////////

#include "boost/tuple/tuple.hpp"
#include "ibtk/FEDataManager.h"
#include "ibtk/libmesh_utilities.h"
#include "libmesh/mesh.h"
#include "libmesh/petsc_vector.h"
#include "libmesh/point.h"
#include "libmesh/system.h"
#include "libmesh/vector_value.h"
#include "libmesh/periodic_boundary.h"
#include "petscsys.h"

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * \brief Class IBFEPostProcessor is a generic interface for specifying the
 * implementation details of a particular post processing algorithm for the
 * IB/FE scheme.
 */
class IBFEPostProcessor
{
public:
    /*!
     * \brief Function for reconstructing the deformation gradient tensor FF =
     * dX/ds.
     */
    static inline void
    FF_fcn(
        libMesh::TensorValue<double>& FF_out,
        const libMesh::TensorValue<double>& FF_in,
        const libMesh::Point& /*X*/,
        const libMesh::Point& /*s*/,
        libMesh::Elem* /*elem*/,
        libMesh::NumericVector<double>& /*X_vec*/,
        const std::vector<libMesh::NumericVector<double>*>& /*system_data*/,
        double /*data_time*/,
        void* /*ctx*/)
        {
            FF_out = FF_in;
            return;
        }// FF_fcn

    /*!
     * \brief Function for reconstructing the Green-Lagrangian strain tensor EE
     * = 0.5*(CC - II), with CC = FF^T FF and FF = dX/ds.
     */
    static inline void
    EE_fcn(
        libMesh::TensorValue<double>& EE,
        const libMesh::TensorValue<double>& FF,
        const libMesh::Point& /*X*/,
        const libMesh::Point& /*s*/,
        libMesh::Elem* /*elem*/,
        libMesh::NumericVector<double>& /*X_vec*/,
        const std::vector<libMesh::NumericVector<double>*>& /*system_data*/,
        double /*data_time*/,
        void* /*ctx*/)
        {
            const libMesh::TensorValue<double> CC = FF.transpose() * FF;
            static const libMesh::TensorValue<double> II(1.0,0.0,0.0,
                                                         0.0,1.0,0.0,
                                                         0.0,0.0,1.0);
            EE = 0.5*(CC - II);
            return;
        }// EE_fcn

    /*!
     * \brief Function for reconstructing the Cauchy stress from the PK1 stress,
     * using the PK1 stress function data provided by the ctx argument.
     */
    static inline void
    cauchy_stress_from_PK1_stress_fcn(
        libMesh::TensorValue<double>& sigma,
        const libMesh::TensorValue<double>& FF,
        const libMesh::Point& X,
        const libMesh::Point& s,
        libMesh::Elem* elem,
        libMesh::NumericVector<double>& X_vec,
        const std::vector<libMesh::NumericVector<double>*>& system_data,
        double data_time,
        void* ctx)
        {
            TBOX_ASSERT(ctx);
            std::pair<IBTK::TensorMeshFcnPtr,void*>* PK1_stress_fcn_data = static_cast<std::pair<IBTK::TensorMeshFcnPtr,void*>*>(ctx);
            TBOX_ASSERT(PK1_stress_fcn_data);
            IBTK::TensorMeshFcnPtr PK1_stress_fcn = PK1_stress_fcn_data->first;
            void* PK1_stress_fcn_ctx = PK1_stress_fcn_data->second;
            libMesh::TensorValue<double> PP;
            PK1_stress_fcn(PP, FF, X, s, elem, X_vec, system_data, data_time, PK1_stress_fcn_ctx);
            sigma = PP * FF.transpose() / FF.det();
            return;
        }// cauchy_stress_from_PK1_stress_fcn

    /*!
     * \brief Function for reconstructing a deformed material axis.  A pointer
     * to the system number must be passed as the ctx argument.
     *
     * \note Assumes that the material axis is described by a piecewise constant
     * field.
     */
    static inline void
    deformed_material_axis_fcn(
        libMesh::VectorValue<double>& f,
        const libMesh::TensorValue<double>& FF,
        const libMesh::Point& /*X*/,
        const libMesh::Point& /*s*/,
        libMesh::Elem* elem,
        libMesh::NumericVector<double>& /*X_vec*/,
        const std::vector<libMesh::NumericVector<double>*>& system_data,
        double /*data_time*/,
        void* ctx)
        {
            TBOX_ASSERT(system_data.size() == 1);
            TBOX_ASSERT(ctx);
            int f0_system_num = *static_cast<int*>(ctx);
            NumericVector<double>* f0_vec = system_data[0];
            VectorValue<double> f0;
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                f0(d) = (*f0_vec)(elem->dof_number(f0_system_num,d,0));
            }
            f = FF*f0;
            return;
        }// deformed_material_axis_fcn

    /*!
     * \brief Function for reconstructing a deformed, normalized material axis.
     * A pointer to the system number must be passed as the ctx argument.
     *
     * \note Assumes that the material axis is described by a piecewise constant
     * field.
     */
    static inline void
    deformed_normalized_material_axis_fcn(
        libMesh::VectorValue<double>& f,
        const libMesh::TensorValue<double>& FF,
        const libMesh::Point& /*X*/,
        const libMesh::Point& /*s*/,
        libMesh::Elem* elem,
        libMesh::NumericVector<double>& /*X_vec*/,
        const std::vector<libMesh::NumericVector<double>*>& system_data,
        double /*data_time*/,
        void* ctx)
        {
            TBOX_ASSERT(system_data.size() == 1);
            TBOX_ASSERT(ctx);
            int f0_system_num = *static_cast<int*>(ctx);
            NumericVector<double>* f0_vec = system_data[0];
            VectorValue<double> f0;
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                f0(d) = (*f0_vec)(elem->dof_number(f0_system_num,d,0));
            }
            f = (FF*f0).unit();
            return;
        }// deformed_normalized_material_axis_fcn

    /*!
     * \brief Function for reconstructing the stretch in a material axis.  A
     * pointer to the system number must be passed as the ctx argument.
     *
     * \note Assumes that the material axis is described by a piecewise constant
     * field.
     */
    static inline void
    material_axis_stretch_fcn(
        double& lambda,
        const libMesh::TensorValue<double>& FF,
        const libMesh::Point& /*X*/,
        const libMesh::Point& /*s*/,
        libMesh::Elem* elem,
        libMesh::NumericVector<double>& /*X_vec*/,
        const std::vector<libMesh::NumericVector<double>*>& system_data,
        double /*data_time*/,
        void* ctx)
        {
            TBOX_ASSERT(system_data.size() == 1);
            TBOX_ASSERT(ctx);
            int f0_system_num = *static_cast<int*>(ctx);
            NumericVector<double>* f0_vec = system_data[0];
            VectorValue<double> f0;
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                f0(d) = (*f0_vec)(elem->dof_number(f0_system_num,d,0));
            }
            VectorValue<double> f = FF*f0;
            lambda = f.size()/f0.size();
            return;
        }// material_axis_stretch_fcn

    /*!
     * Constructor.
     */
    IBFEPostProcessor(
        libMesh::MeshBase* mesh,
        IBTK::FEDataManager* fe_data_manager);

    /*!
     * Virtual destructor.
     */
    virtual
    ~IBFEPostProcessor();

    /*!
     * Register a scalar-valued variable for reconstruction.
     */
    virtual void
    registerScalarVariable(
        const std::string& var_name,
        libMeshEnums::FEFamily var_fe_family,
        libMeshEnums::Order var_fe_order,
        IBTK::ScalarMeshFcnPtr var_fcn,
        std::vector<unsigned int> var_fcn_systems=std::vector<unsigned int>(),
        void* var_fcn_ctx=NULL);

    /*!
     * Register a vector-valued variable for reconstruction.
     */
    virtual void
    registerVectorVariable(
        const std::string& var_name,
        libMeshEnums::FEFamily var_fe_family,
        libMeshEnums::Order var_fe_order,
        IBTK::VectorMeshFcnPtr var_fcn,
        std::vector<unsigned int> var_fcn_systems=std::vector<unsigned int>(),
        void* var_fcn_ctx=NULL,
        unsigned int var_dim=NDIM);

    /*!
     * Register a tensor-valued variable for reconstruction.
     */
    virtual void
    registerTensorVariable(
        const std::string& var_name,
        libMeshEnums::FEFamily var_fe_family,
        libMeshEnums::Order var_fe_order,
        IBTK::TensorMeshFcnPtr var_fcn,
        std::vector<unsigned int> var_fcn_systems=std::vector<unsigned int>(),
        void* var_fcn_ctx=NULL,
        unsigned int var_dim=NDIM);

    /*!
     * Initialize data used by the post processor.
     */
    virtual void
    initializeFEData();

    /*!
     * Virtual function to reconstruct the data on the mesh.
     */
    virtual void
    reconstructVariables(
        double data_time) = 0;

protected:
    /*!
     * Mesh data.
     */
    libMesh::MeshBase* d_mesh;
    IBTK::FEDataManager* d_fe_data_manager;
    bool d_fe_data_initialized;

    /*!
     * Scalar-valued reconstruction data.
     */
    std::vector<libMesh::System*> d_scalar_var_systems;
    std::vector<IBTK::ScalarMeshFcnPtr> d_scalar_var_fcns;
    std::vector<std::vector<unsigned int> > d_scalar_var_fcn_systems;
    std::vector<void*> d_scalar_var_fcn_ctxs;

    /*!
     * Vector-valued reconstruction data.
     */
    std::vector<libMesh::System*> d_vector_var_systems;
    std::vector<IBTK::VectorMeshFcnPtr> d_vector_var_fcns;
    std::vector<std::vector<unsigned int> > d_vector_var_fcn_systems;
    std::vector<void*> d_vector_var_fcn_ctxs;
    std::vector<unsigned int> d_vector_var_dims;

    /*!
     * Tensor-valued reconstruction data.
     */
    std::vector<libMesh::System*> d_tensor_var_systems;
    std::vector<IBTK::TensorMeshFcnPtr> d_tensor_var_fcns;
    std::vector<std::vector<unsigned int> > d_tensor_var_fcn_systems;
    std::vector<void*> d_tensor_var_fcn_ctxs;
    std::vector<unsigned int> d_tensor_var_dims;

    /*!
     * Collection of all systems managed by this object.
     */
    std::vector<libMesh::System*> d_var_systems;

    /*!
     * Collection of all systems required by mesh functions registered with this
     * object.
     */
    std::set<unsigned int> d_var_fcn_systems;

private:
    /*!
     * \brief Default constructor.
     *
     * \note This constructor is not implemented and should not be used.
     */
    IBFEPostProcessor();

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    IBFEPostProcessor(
        const IBFEPostProcessor& from);

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    IBFEPostProcessor&
    operator=(
        const IBFEPostProcessor& that);
};
}// namespace IBAMR

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBFEPostProcessor
