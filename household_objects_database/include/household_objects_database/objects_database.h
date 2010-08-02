/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

// Author(s): Matei Ciocarlie

#ifndef _OBJECTS_DATABASE_H_
#define _OBJECTS_DATABASE_H_

//for ROS error messages
#include <ros/ros.h>

#include <geometric_shapes_msgs/Shape.h>

#include <database_interface/postgresql_database.h>

#include "household_objects_database/database_original_model.h"
#include "household_objects_database/database_scaled_model.h"
#include "household_objects_database/database_grasp.h"
#include "household_objects_database/database_mesh.h"
#include "household_objects_database/database_perturbation.h"

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

namespace household_objects_database {

class DatabaseTask;

//! A slight specialization of the general database interface with a few convenience functions added
class ObjectsDatabase : public database_interface::PostgresqlDatabase
{
 public:
  //! Attempts to connect to the specified database
  ObjectsDatabase(std::string host, std::string port, std::string user,
		  std::string password, std::string dbname) 
    : PostgresqlDatabase(host, port, user, password, dbname) {}

  //! Attempts to connect to the specified database
  ObjectsDatabase(const database_interface::PostgresqlDatabaseConfig &config) 
    : PostgresqlDatabase(config) {}

  //! Just a stub for now
  ~ObjectsDatabase() {}

  //! Acquires the next experiment to be executed from the list of tasks in the database
  /*! Also marks it as RUNNING in an atomic fashion, so that it is not acquired by 
   another process.*/
  virtual bool acquireNextTask(std::vector< boost::shared_ptr<DatabaseTask> > &task);

  //------- helper functions wrapped around the general versions for convenience -------
  //----------------- or for cases where where_clauses are needed ----------------------

  //! Gets a list of all the original models in the database
  bool getOriginalModelsList(std::vector< boost::shared_ptr<DatabaseOriginalModel> > &models) const
  {
    DatabaseOriginalModel example;
    return getList<DatabaseOriginalModel>(models, example, "");
  }

  //! Gets a list of all the scaled models in the database
  bool getScaledModelsList(std::vector< boost::shared_ptr<DatabaseScaledModel> > &models) const
  {
    DatabaseScaledModel example;
    return getList<DatabaseScaledModel>(models, example, "");
  }

  //! Gets a list of scaled models based on acquisition method
  bool getScaledModelsByAcquisition(std::vector< boost::shared_ptr<DatabaseScaledModel> > &models, 
				    std::string acquisition_method) const
  {
    std::string where_clause("acquisition_method_name='" + acquisition_method + "'");
    DatabaseScaledModel example;
    //this should be set by default, but let's make sure again
    example.acquisition_method_.setReadFromDatabase(true);
    return getList<DatabaseScaledModel>(models, example, where_clause);
  }

  virtual bool getScaledModelsBySet(std::vector< boost::shared_ptr<DatabaseScaledModel> > &models,
				    std::string model_set_name) const
  {
    if (model_set_name.empty()) return getScaledModelsList(models);
    std::string where_clause = std::string("original_model_id IN (SELECT original_model_id FROM "
					   "model_set WHERE model_set_name = '") + 
      model_set_name + std::string("')");
    DatabaseScaledModel example;
    return getList<DatabaseScaledModel>(models, example, where_clause);
  }

  //! Returns the number of original models in the database
  bool getNumOriginalModels(int &num_models) const
  {
    DatabaseOriginalModel example;
    return countList(&example, num_models, "");
  }

  //! Returns the path that geometry paths are relative to
  bool getModelRoot(std::string& root) const
  {
    return getVariable("'MODEL_ROOT'", root);
  }

  //! Gets a list of all models with the requested tags in the database
  bool getModelsListByTags(std::vector< boost::shared_ptr<DatabaseOriginalModel> > &models, 
			   std::vector<std::string> tags) const
  {
    DatabaseOriginalModel example;
    std::string where_clause("(");
    for (size_t i=0; i<tags.size(); i++)
    {
      if (i!=0) where_clause += "AND ";
      where_clause += "'" + tags[i] + "' = ANY (original_model_tags)";
    }
    where_clause += ")";
    return getList<DatabaseOriginalModel>(models, example, where_clause);    
  }

  //! Gets the list of all the grasps for a scaled model id
  bool getGrasps(int scaled_model_id, std::string hand_name, 
		 std::vector< boost::shared_ptr<DatabaseGrasp> > &grasps) const
  {
    DatabaseGrasp example;
    std::stringstream id;
    id << scaled_model_id;
    std::string where_clause("scaled_model_id=" + id.str() + 
			     " AND hand_name='" + hand_name + "'");
    return getList<DatabaseGrasp>(grasps, example, where_clause);
  }

  //! Gets the list of only those grasps that are cluster reps a database model
  bool getClusterRepGrasps(int scaled_model_id, std::string hand_name, 
			   std::vector< boost::shared_ptr<DatabaseGrasp> > &grasps) const
  {
    DatabaseGrasp example;
    std::stringstream id;
    id << scaled_model_id;
    std::string where_clause("scaled_model_id=" + id.str() + 
			     " AND hand_name='" + hand_name + "'" + 
			     " AND grasp_cluster_rep=true");
    return getList<DatabaseGrasp>(grasps, example, where_clause);
  }

  //! Gets  the mesh for a scaled model
  bool getScaledModelMesh(int scaled_model_id, DatabaseMesh &mesh) const
  {
    //first get the original model id
    DatabaseScaledModel scaled_model;
    scaled_model.id_.data() = scaled_model_id;
    if (!loadFromDatabase(&scaled_model.original_model_id_))
    {
      ROS_ERROR("Failed to get original model for scaled model id %d", scaled_model_id);
      return false;
    }
    mesh.id_.data() = scaled_model.original_model_id_.data();
    if ( !loadFromDatabase(&mesh.triangles_) ||
	 !loadFromDatabase(&mesh.vertices_) )
    {
      ROS_ERROR("Failed to load mesh from database for scaled model %d, resolved to original model %d",
		scaled_model_id, mesh.id_.data());
      return false;
    }
    return true;
  }

  //! Gets the mesh for a scaled model as a geometric_shapes_msgs::Shape
  bool getScaledModelMesh(int scaled_model_id, geometric_shapes_msgs::Shape &shape) const
  {
    DatabaseMesh mesh;
    if ( !getScaledModelMesh(scaled_model_id, mesh) ) return false;
    shape.triangles = mesh.triangles_.data();
    shape.vertices.clear();
    if ( mesh.vertices_.data().size() % 3 != 0 )
    {
      ROS_ERROR("Get scaled model mesh: size of vertices vector is not a multiple of 3");
      return false;
    }
    for (size_t i=0; i<mesh.vertices_.data().size()/3; i++)
    {
      geometry_msgs::Point p;
      p.x = mesh.vertices_.data().at(3*i+0);
      p.y = mesh.vertices_.data().at(3*i+1);
      p.z = mesh.vertices_.data().at(3*i+2);
      shape.vertices.push_back(p);
    }
    return true;
  }

/*!
 * These two functions use the ANY(ARRAY[ids]) syntax because those were the most performant in speed tests.
 */
  //! Gets the perturbations for all grasps for a given scaled model
  bool getAllPerturbationsForModel(int scaled_model_id, std::vector<DatabasePerturbationPtr> &perturbations)
  {
    std::string where_clause = std::string("grasp_id = ANY(ARRAY(SELECT grasp_id FROM grasp WHERE "
                                            "scaled_model_id = " + boost::lexical_cast<std::string>(scaled_model_id) +std::string("))"));
    DatabasePerturbation example;
    return getList<DatabasePerturbation>(perturbations, example, where_clause);
  }

  bool getPerturbationsForGrasps(const std::vector<int> &grasp_ids, std::vector<DatabasePerturbationPtr> &perturbations)
  {
    std::vector<std::string> grasp_id_strs;
    grasp_id_strs.reserve(grasp_ids.size());
    BOOST_FOREACH(int id, grasp_ids) {
      grasp_id_strs.push_back(boost::lexical_cast<std::string,int>(id));
    }
    std::string where_clause = std::string("grasp_id = ANY(ARRAY["+boost::algorithm::join(grasp_id_strs, ", ")+"])");
    DatabasePerturbation example;
    return getList<DatabasePerturbation>(perturbations, example, where_clause);
  }
};
typedef boost::shared_ptr<ObjectsDatabase> ObjectsDatabasePtr;
}//namespace

#endif
