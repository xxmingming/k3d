// K-3D
// Copyright (c) 1995-2007, Timothy M. Shead
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
	\author Carlos Andres Dominguez Caballero (carlos@gmail.com)
*/

//#include "gprim_factory.h"
#include <k3d-i18n-config.h>
#include <k3dsdk/document_plugin_factory.h>
#include <k3dsdk/fstream.h>
#include <k3dsdk/gprim_factory.h>
#include <k3dsdk/imesh_storage.h>
#include <k3dsdk/mesh_source.h>
#include <k3dsdk/node.h>
#include <k3dsdk/xml.h>
//#include <k3dsdk/gprim_factory.h>

namespace module
{

namespace svg
{

namespace io
{

/////////////////////////////////////////////////////////////////////////////
// mesh_reader_implementation

class mesh_reader_implementation :
	public k3d::mesh_source<k3d::node >,
	public k3d::imesh_storage
{
	typedef k3d::mesh_source<k3d::node > base;

public:
	mesh_reader_implementation(k3d::iplugin_factory& Factory, k3d::idocument& Document) :
		base(Factory, Document),
		m_file(init_owner(*this) + init_name("file") + init_label(_("File")) + init_description(_("Input file")) + init_value(k3d::filesystem::path()) + init_path_mode(k3d::ipath_property::READ) + init_path_type("svg_files"))
	{
		m_file.changed_signal().connect(make_topology_changed_slot());
		count = 0;
	}

	void reset_mesh(k3d::mesh* const Mesh)
	{
		m_output_mesh.reset(Mesh);
	}

	void on_create_mesh_topology(k3d::mesh& Mesh)
	{
		const k3d::filesystem::path svg_path = m_file.pipeline_value();
		if(svg_path.empty())
			return;

		k3d::xml::element xml_svg;

		try
		{
			k3d::log() << info << "Reading SVG file from " << svg_path.native_console_string() << std::endl;
			k3d::filesystem::ifstream svg_stream(svg_path);
			svg_stream >> xml_svg;
			assert_warning(xml_svg.name == "svg");
		}
		catch(...)
		{
			k3d::log() << error << "Error reading SVG file from " << svg_path.native_console_string() << std::endl;
		}

		factory = new k3d::gprim_factory(Mesh);
		count = 0;

		parse_graphics(xml_svg);
		
	}

	void on_update_mesh_geometry(k3d::mesh& Mesh)
	{
	}

	static k3d::iplugin_factory& get_factory()
	{
		static k3d::document_plugin_factory<mesh_reader_implementation,
                k3d::interface_list<k3d::imesh_source,
                k3d::interface_list<k3d::imesh_storage> > > factory(
			k3d::uuid(0xa0149217, 0xf64844bb, 0x4ba01e8f, 0x21d7f651),
			"SVGMeshReader",
			_("Reader that loads external Scalable Vector Graphics (.svg) files into the document by reference"),
			"MeshReader");

		return factory;
	}

private:
	k3d_data(k3d::filesystem::path, immutable_name, change_signal, with_undo, local_storage, no_constraint, path_property, path_serialization) m_file;
	k3d::gprim_factory *factory;
	int count;

	void parse_graphics(const k3d::xml::element& xml_root)
	{
		for(k3d::xml::element::elements_t::const_iterator xml_obj = xml_root.children.begin(); xml_obj != xml_root.children.end(); ++xml_obj)
		{
			if(xml_obj->name == "path")
				parse_path(*xml_obj);
			if(xml_obj->name == "rect")
				parse_rect(*xml_obj);
			if(xml_obj->name == "g")
				parse_graphics(*xml_obj);
		}
	}

	void parse_rect(const k3d::xml::element& xml_obj)
	{	
		double x, y, w, h;
		k3d::mesh::indices_t rect;
		const std::string xx = k3d::xml::attribute_text(xml_obj, "x");
		const std::string yy = k3d::xml::attribute_text(xml_obj, "y");
		const std::string width = k3d::xml::attribute_text(xml_obj, "width");
		const std::string height = k3d::xml::attribute_text(xml_obj, "height");

		x = atof(xx.c_str());
		y = atof(yy.c_str());
		w = atof(width.c_str());
		h = atof(height.c_str());

		for(int i=0; i<4; i++)
		{
			rect.push_back(count);
			count++;
		}
		//rect.push_back(rect.front());
		factory->add_point(k3d::point4(x,y,0,1));
		factory->add_point(k3d::point4(x+w,y,0,1));
		factory->add_point(k3d::point4(x+w,y+h,0,1));
		factory->add_point(k3d::point4(x,y+h,0,1));
		
		//factory->add_curve(rect,2);
		factory->add_polygon(rect);		
	
	}

	bool is_together(const std::string& arg)
	{
		if(arg.find(",",1)>arg.length())
			return false;
		return true;
	}

	void get_pair(float& x, float& y, std::istringstream& def_stream)
	{
		std::string arg;
		def_stream >> arg;
		if(is_together(arg))
			sscanf(arg.c_str(), "%f,%f", &x, &y);
		else
		{
			x = atof(arg.c_str());
			def_stream >> y;
		}
	}

	void parse_path(const k3d::xml::element& xml_obj)
	{
		const std::string def_path = k3d::xml::attribute_text(xml_obj, "d");
		char token;
		float x, y;
		int last, first;
		bool relative=false;
		k3d::point4 slastpoint;
		k3d::point4 lastpoint;
		k3d::point4 firstpoint;

		std::istringstream def_stream(def_path);

		def_stream >> token;

		if(token != 'M')
			k3d::log() << error << "Error parsing path " << k3d::xml::attribute_text(xml_obj, "id") << " missing start point." << std::endl;

		get_pair(x,y,def_stream);

		last = first = count;
		count++;
		firstpoint = k3d::point4(x,y,0,1);
		factory->add_point(firstpoint);

		while(!def_stream.eof())
		{
			k3d::uint_t order = 3;
			k3d::mesh::indices_t points;
            k3d::mesh::knots_t knots;
			k3d::mesh::weights_t weights;
			points.push_back(last);
			def_stream >> token;
			switch(token)
			{
			case 'l':
				relative = true;
			case 'L':
				order = 2;
				slastpoint = k3d::point4(x,y,0,1);
				get_pair(x,y,def_stream);
				points.push_back(count);
				count++;
				if(relative)
				{
					x+=lastpoint[0];
					y+=lastpoint[1];
				}
				lastpoint = k3d::point4(x,y,0,1);
				factory->add_point(lastpoint);
				last = points.back();
				break;
			case 'h':
				relative = true;
			case 'H':
				order = 2;
				slastpoint = k3d::point4(x,y,0,1);
				def_stream >> x;
				points.push_back(count);
				count++;
				if(relative)
					x+=lastpoint[0];
				lastpoint = k3d::point4(x,lastpoint[1],0,1);
				factory->add_point(lastpoint);
				last = points.back();
				break;
			case 'v':
				relative = true;
			case 'V':
				order = 2;
				slastpoint = k3d::point4(x,y,0,1);
				def_stream >> y;
				points.push_back(count);
				count++;
				if(relative)
					y+=lastpoint[1];
				lastpoint = k3d::point4(lastpoint[0], y, 0, 1);
				factory->add_point(lastpoint);
				last = points.back();
				break;
			case 'Z':
			case 'z':
				order = 2;
				slastpoint = k3d::point4(x,y,0,1);
				lastpoint = firstpoint;
				points.push_back(first);
				break;
			case 'c':
				relative = true;
			case 'C':
				order = 4;
				for(int i=0; i<3; i++)
				{
					get_pair(x,y,def_stream);
					points.push_back(count);
					count++;
					if(relative)
					{
						x += lastpoint[0];
						y += lastpoint[1];
					}
					factory->add_point(k3d::point4(x,y,0,1));
					if(i==1)
						slastpoint = k3d::point4(x,y,0,1);
				}
				lastpoint = k3d::point4(x,y,0,1);
				last = points.back();
				break;
			case 's':
				relative = true;
			case 'S':
				order = 4;
				points.push_back(count);
				count++;
				factory->add_point(k3d::point4(2*lastpoint[0]-slastpoint[0], 2*lastpoint[1]-slastpoint[1],0,1));
				for(int i=0; i<2; i++)
				{
					get_pair(x,y,def_stream);
					points.push_back(count);
					count++;
					if(relative)
					{
						x+=lastpoint[0];
						y+=lastpoint[1];
					}
					factory->add_point(k3d::point4(x,y,0,1));
					if(i==0)
						slastpoint = k3d::point4(x,y,0,1);
				}
				lastpoint = k3d::point4(x,y,0,1);
				last = points.back();
				break;
			case 'q':
				relative = true;
			case 'Q':
				order = 3;
				for(int i=0; i<2; i++)
				{
					get_pair(x,y,def_stream);
					points.push_back(count);
					count++;
					if(relative)
					{
						x += lastpoint[0];
						y += lastpoint[1];
					}
					factory->add_point(k3d::point4(x,y,0,1));
					if(i==0)
						slastpoint = k3d::point4(x,y,0,1);
				}
				lastpoint = k3d::point4(x,y,0,1);
				last = points.back();
				break;
			case 't':
				relative = true;
			case 'T':
				order = 3;
				points.push_back(count);
				count++;
				slastpoint =k3d::point4(2*lastpoint[0]-slastpoint[0], 2*lastpoint[1]-slastpoint[1],0,1);
				factory->add_point(slastpoint);

				get_pair(x,y,def_stream);
				points.push_back(count);
				count++;
				if(relative)
				{
					x+=lastpoint[0];
					y+=lastpoint[1];
				}
				factory->add_point(k3d::point4(x,y,0,1));
				lastpoint = k3d::point4(x,y,0,1);
				last = points.back();
				break;
			}


			//Start knot Vector Definition to be a Bezier curve
			knots.push_back(0);
			for(int i=0; i < order-1; i++)
				knots.push_back(1);
			for(int i=0; i < order-1; i++)
				knots.push_back(2);
			knots.push_back(3);
			//End knot vector definition

			//Start weight vector definition as bezier curve
			for(k3d::uint_t point = 0; point < points.size(); point++ )
				weights.push_back(1.0);
			//End weight vector definition

			factory->add_nurbs_curve(order, points, knots, weights);

		}
		relative = false;

	}

};

k3d::iplugin_factory& mesh_reader_factory()
{
	return mesh_reader_implementation::get_factory();
}

} // namespace io

} // namespace svg

} // namespace module
