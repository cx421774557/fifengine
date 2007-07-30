/***************************************************************************
 *   Copyright (C) 2005-2007 by the FIFE Team                              *
 *   fife-public@lists.sourceforge.net                                     *
 *   This file is part of FIFE.                                            *
 *                                                                         *
 *   FIFE is free software; you can redistribute it and/or modify          *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA              *
 ***************************************************************************/

#ifndef FIFE_ELEVATION_H
#define FIFE_ELEVATION_H

// Standard C++ library includes
#include <algorithm>
#include <vector>
#include <string>

// 3rd party library includes

// FIFE includes
// These includes are split up in two parts, separated by one empty line
// First block: files included from the FIFE root src directory
// Second block: files included from the same folder
#include "util/attributedclass.h"

namespace FIFE { namespace model {

	class Layer;
	class Map;

	class GeometryType;

	/** Contains a game level.
	 * 
	 * \invariant getLayer(index)->getLayerNumber() == index
	 * \invariant elevation->addLayer(layer); layer->getElevation() == elevation
	 * \invariant elevation->insertLayer(index,layer); layer->getElevation() == elevation
	 * \invariant elevation->removeLayer(layer->getLayerNumber()); 
	 *  layer->getElevation() == ElevationPtr()
	 */
	class Elevation : public AttributedClass {
		public:

			/** Destructor
			 */
			~Elevation();

			/** Retrieve the map this elevation is contained in
			 */
			Map* getMap();

			/** Add a Layer to this Elevation. Elevation owns
			 * the returned pointer to the new Layer, so don't
			 * delete it!
			 */
			Layer* addLayer(const Point& size, GeometryType* geometry);

			/** Remove a layer from the elevation
			 */
			void removeLayer(Layer*);

			/** Get a layer by a value. If multiple Layers match this
			 * value, then an arbitrary matching layer is returned.
			 * @param the field to search on
			 * @param the value to be searched for in the field
			 */
			template<typename T>
			Layer* getLayer(const std::string& field, const T& value) const;

			/** Get the overall number of layers
			 */
			size_t getNumLayers() const;

			/** Remove all layers from the elevation
			 */
			void clearLayers();

			/** Apply a visitor to each layer
			 */
			template<typename T>
			void forEachLayer(T visitor) {
				std::for_each(m_layers.begin(),m_layers.end(),visitor);
			}

			/** Set the 'reference' Layer
			 */
			void setReferenceLayer(Layer* layer);

			/** Get the 'reference' Layer
			 */
			Layer* getReferenceLayer();

			/** Try to calculate a valid nice starting position
			 */
			Point centerOfMass();

		private:

			/** Constructor (Elevations are created by Maps)
			 */
			Elevation();
		
			std::string m_name;

			Map* m_map;

			std::vector<Layer*> m_layers;

			int m_width;
			int m_height;
			Layer* m_reference_layer;

			// Not copyable.
			Elevation(const Elevation&);
			Elevation& operator=(const Elevation&);

			friend class Map;
	};

} } //FIFE::model

#endif
/* vim: set noexpandtab: set shiftwidth=2: set tabstop=2: */
