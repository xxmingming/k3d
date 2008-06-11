// K-3D
// Copyright (c) 1995-2004, Timothy M. Shead
//
// Contact: tshead@k-3d.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/** \file
		\author Carsten Haubold (CarstenHaubold@web.de)
*/

#include <k3dsdk/document_plugin_factory.h>
#include <k3dsdk/log.h>
#include <k3dsdk/module.h>
#include <k3dsdk/node.h>
#include <k3dsdk/mesh.h>
#include <k3dsdk/mesh_source.h>
#include <k3dsdk/material_sink.h>
#include <k3dsdk/mesh_operations.h>
#include <k3dsdk/nurbs.h>
#include <k3dsdk/measurement.h>
#include <k3dsdk/selection.h>
#include <k3dsdk/data.h>
#include <k3dsdk/point3.h>
#include <k3dsdk/mesh_modifier.h>
#include <k3dsdk/mesh_selection_sink.h>
#include <k3dsdk/shared_pointer.h>

#include <iostream>
#include <vector>

namespace module
{

	namespace nurbs
	{
		void connectAtPoints(k3d::mesh& Mesh, size_t curve1, size_t curve2, size_t point1, size_t point2, bool continuous);


		class connect_curves :
			public k3d::mesh_selection_sink<k3d::mesh_modifier<k3d::node > >
		{
			typedef k3d::mesh_selection_sink<k3d::mesh_modifier<k3d::node > > base;
		public:
			connect_curves(k3d::iplugin_factory& Factory, k3d::idocument& Document) :
				base(Factory, Document),
				m_make_continuous(init_owner(*this) + init_name("make_continuous") + init_label(_("make_continuous")) + init_description(_("Connect Continuous? Resets the Knot-Vector!")) + init_value(false) )
			{
				m_mesh_selection.changed_signal().connect(make_update_mesh_slot());
				m_make_continuous.changed_signal().connect(make_update_mesh_slot());
			}

			void on_create_mesh(const k3d::mesh& Input, k3d::mesh& Output) 
			{
				Output = Input;
				k3d::bool_t make_continuous = m_make_continuous.internal_value();
				
				if(!k3d::validate_nurbs_curve_groups(Output))
					return;

				merge_selection(m_mesh_selection.pipeline_value(), Output);

				std::vector<size_t> curves;
				std::vector<size_t> points;
				
				const size_t group_begin = 0;
				const size_t group_end = group_begin + (*Output.nurbs_curve_groups->first_curves).size();
				for(size_t group = group_begin; group != group_end; ++group)
				{
					const size_t curve_begin = (*Output.nurbs_curve_groups->first_curves)[group];
					const size_t curve_end = curve_begin + (*Output.nurbs_curve_groups->curve_counts)[group];
					for(size_t curve = curve_begin; curve != curve_end; ++curve)
					{
						const size_t curve_point_begin = (*Output.nurbs_curve_groups->curve_first_points)[curve];
						const size_t curve_point_end = curve_point_begin + (*Output.nurbs_curve_groups->curve_point_counts)[curve];

						const k3d::mesh::weights_t& orig_weights = *Output.nurbs_curve_groups->curve_point_weights;
						
						boost::shared_ptr<k3d::mesh::weights_t> curve_point_weights ( new k3d::mesh::weights_t() );
						
						for(size_t curve_point = curve_point_begin; curve_point != curve_point_end; ++curve_point)
						{
							if((*Output.point_selection)[(*Output.nurbs_curve_groups->curve_points)[curve_point]])
							{
								curves.push_back(curve);
								points.push_back(curve_point);
							}
						}
					}
				}
				
				if( curves.size() != 2 || points.size() != 2)
				{
					k3d::log() << error << "You need to select exactly 2 points!\n"<<"Selected: "<<points.size()<<" points on "<<curves.size()<<" curves" << std::endl;
				}

				connectAtPoints(Output, curves[0], curves[1], points[0], points[1], make_continuous);

				assert_warning(k3d::validate_nurbs_curve_groups(Output));
			}

			void on_update_mesh(const k3d::mesh& Input, k3d::mesh& Output)
			{
				on_create_mesh(Input,Output);
			}

			static k3d::iplugin_factory& get_factory()
			{
				static k3d::document_plugin_factory<connect_curves, k3d::interface_list<k3d::imesh_source, k3d::interface_list<k3d::imesh_sink > > > factory(
				k3d::uuid(0x959eb84b, 0x544d0672, 0xd53d899b, 0x6a568e86),
					"NurbsConnectCurves",
					_("Connects a set of NURBS curves"),
					"NURBS",
					k3d::iplugin_factory::EXPERIMENTAL);

				return factory;
			}
			
		private:
			k3d_data(k3d::bool_t, immutable_name, change_signal, with_undo, local_storage, no_constraint, writable_property, with_serialization) m_make_continuous;
		};

		//Create connect_curve factory
		k3d::iplugin_factory& connect_curves_factory()
		{
			return connect_curves::get_factory();
		}
		
		
		
		void replace_point(k3d::mesh::nurbs_curve_groups_t& groups, k3d::mesh::indices_t& indices, k3d::mesh::knots_t& knots, size_t newIndex, size_t curve, size_t point, bool continuous)
		{
			const size_t curve_point_begin = (*groups.curve_first_points)[curve];
			const size_t curve_point_end = curve_point_begin + (*groups.curve_point_counts)[curve];
						
			for(size_t points = curve_point_begin; points < curve_point_end; ++points)
			{
				if(indices[points] == point)
				{
					//we found the index pointing to point1
					indices[points] = newIndex;
					if(continuous)
					{
						
						const size_t curve_knots_begin = (*groups.curve_first_knots)[curve];
						const size_t curve_knots_end = curve_knots_begin + curve_point_end - curve_point_begin + (*groups.curve_orders)[curve];

						const size_t order = (*groups.curve_orders)[curve];
						const size_t half_order = static_cast<unsigned int> (floor(0.5 * order));
						const size_t pos = half_order + (points - curve_point_begin) + curve_knots_begin;
						float knot_at_pos = knots[ points - curve_point_begin + curve_knots_begin + half_order ];
						
						if( pos - curve_knots_begin < order )
						{
							for( size_t x = curve_knots_begin; x < order + curve_knots_begin; ++x )
								knots[x] = knot_at_pos;
							size_t knot_pos = order + curve_knots_begin;
							while( (knots[knot_pos + 1] - knots[knot_pos] > 2) && (knot_pos < curve_knots_end -1) )
							{
								knots[knot_pos + 1] = knots[knot_pos] + 2;
								knot_pos++;
							}	
						}
						else if( pos - curve_knots_begin + order < curve_knots_end )
						{
							for( size_t x = curve_knots_end - 1; x > curve_knots_end - order; --x )
								knots[x] = knot_at_pos;
							size_t knot_pos = curve_knots_end - order;
							while( (knots[knot_pos] - knots[knot_pos - 1] > 2) && (knot_pos < curve_knots_begin) )
							{
								knots[knot_pos - 1] = knots[knot_pos] - 2;
								knot_pos--;
							}	
						}
						else
							k3d::log() << debug << "Should split up the curve here" << std::endl;
					}
				}
			}
		}
		
		void connectAtPoints(k3d::mesh& Mesh, size_t curve1, size_t curve2, size_t point1, size_t point2, bool continuous)
		{
						
			//Add the new point
			k3d::mesh::points_t& mesh_points = *k3d::make_unique(Mesh.points);
			k3d::mesh::selection_t& point_selection = *k3d::make_unique(Mesh.point_selection);

			//merge the 2 points into one
			k3d::point3 p1 = mesh_points[point1];
			k3d::point3 p2 = mesh_points[point2];
			
			k3d::point3 new_point = (p1 + p2); //middle of the 2 points
			new_point*=0.5;//has to be calculated according to ending tangential vectors
			
			size_t newIndex = mesh_points.size();
			mesh_points.push_back(new_point);
			point_selection.push_back(0.0);
			
			//ToDo: remove the points if no longer used
			
			//loop through curve point indices to find the given ones and change them so they point to "new"
			k3d::mesh::nurbs_curve_groups_t& groups = *k3d::make_unique(Mesh.nurbs_curve_groups);
			
			k3d::mesh::indices_t& indices = *k3d::make_unique(groups.curve_points);
			k3d::mesh::knots_t& knots = *k3d::make_unique(groups.curve_knots);
			
			//curve1 - point 1
			replace_point(groups, indices, knots, newIndex, curve1, point1, continuous);
			
			
			//curve2 - point2
			replace_point(groups, indices, knots, newIndex, curve2, point2, continuous);
		}

	}//namespace nurbs
} //namespace module
