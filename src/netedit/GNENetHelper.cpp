/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2021 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNENetHelper.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Mar 2020
///
// Helper for GNENet
/****************************************************************************/

#include <netbuild/NBAlgorithms.h>
#include <netbuild/NBNetBuilder.h>
#include <netedit/GNENet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEViewParent.h>
#include <netedit/changes/GNEChange_Shape.h>
#include <netedit/frames/network/GNECreateEdgeFrame.h>
#include <netedit/elements/additional/GNEPOI.h>
#include <netedit/elements/additional/GNEPoly.h>
#include <netedit/elements/data/GNEDataInterval.h>
#include <netedit/elements/demand/GNEVehicleType.h>
#include <netedit/elements/network/GNEEdgeType.h>

#include "GNENetHelper.h"

// ---------------------------------------------------------------------------
// GNENetHelper::AttributeCarriers - methods
// ---------------------------------------------------------------------------

GNENetHelper::AttributeCarriers::AttributeCarriers(GNENet* net) :
    myNet(net),
    myAllowUndoShapes(true) {
    // fill additionals with tags
    auto additionalTags = GNEAttributeCarrier::getAllowedTagsByCategory(GNETagProperties::TagType::ADDITIONALELEMENT | GNETagProperties::TagType::SYMBOL, false);
    for (const auto& additionalTag : additionalTags) {
        myAdditionals.insert(std::make_pair(additionalTag.first, std::map<std::string, GNEAdditional*>()));
    }
    // fill shapes with tags
    auto shapeTags = GNEAttributeCarrier::getAllowedTagsByCategory(GNETagProperties::TagType::SHAPE, false);
    for (const auto& shapeTag : shapeTags) {
        myShapes.insert(std::make_pair(shapeTag.first, std::map<std::string, GNEShape*>()));
    }
    // fill TAZElements with tags
    auto TAZElementTags = GNEAttributeCarrier::getAllowedTagsByCategory(GNETagProperties::TagType::TAZELEMENT, false);
    for (const auto& TAZElementTag : TAZElementTags) {
        myTAZElements.insert(std::make_pair(TAZElementTag.first, std::map<std::string, GNETAZElement*>()));
    }
    // fill demand elements with tags
    auto demandElementTags = GNEAttributeCarrier::getAllowedTagsByCategory(GNETagProperties::TagType::DEMANDELEMENT, false);
    for (const auto& demandElementTag : demandElementTags) {
        myDemandElements.insert(std::make_pair(demandElementTag.first, std::map<std::string, GNEDemandElement*>()));
    }
    auto stopTags = GNEAttributeCarrier::getAllowedTagsByCategory(GNETagProperties::TagType::STOP, false);
    for (const auto& stopTag : stopTags) {
        myDemandElements.insert(std::make_pair(stopTag.first, std::map<std::string, GNEDemandElement*>()));
    }
}


GNENetHelper::AttributeCarriers::~AttributeCarriers() {
    // Drop EdgeTypes
    for (const auto& edgeType : myEdgeTypes) {
        edgeType.second->decRef("GNENetHelper::~GNENet");
        // show extra information for tests
        WRITE_DEBUG("Deleting unreferenced " + edgeType.second->getTagStr() + " '" + edgeType.second->getID() + "' in AttributeCarriers destructor");
        delete edgeType.second;
    }
    // Drop Edges
    for (const auto& edge : myEdges) {
        edge.second->decRef("GNENetHelper::~GNENet");
        // show extra information for tests
        WRITE_DEBUG("Deleting unreferenced " + edge.second->getTagStr() + " '" + edge.second->getID() + "' in AttributeCarriers destructor");
        delete edge.second;
    }
    // Drop myJunctions
    for (const auto& junction : myJunctions) {
        junction.second->decRef("GNENetHelper::~GNENet");
        // show extra information for tests
        WRITE_DEBUG("Deleting unreferenced " + junction.second->getTagStr() + " '" + junction.second->getID() + "' in AttributeCarriers destructor");
        delete junction.second;
    }
    // Drop Additionals (Only used for additionals that were inserted without using GNEChange_Additional)
    for (const auto& additionalTag : myAdditionals) {
        for (const auto& additional : additionalTag.second) {
            // decrease reference manually (because it was increased manually in GNEAdditionalHandler)
            additional.second->decRef();
            // show extra information for tests
            WRITE_DEBUG("Deleting unreferenced " + additional.second->getTagStr() + " '" + additional.second->getID() + "' in AttributeCarriers destructor");
            delete additional.second;
        }
    }
    // Drop Shapes (Only used for shapes that were inserted without using GNEChange_Shape)
    for (const auto& shapeTag : myShapes) {
        for (const auto& shape : shapeTag.second) {
            // decrease reference manually (because it was increased manually in GNEShapeHandler)
            shape.second->decRef();
            // show extra information for tests
            WRITE_DEBUG("Deleting unreferenced " + shape.second->getTagStr() + " '" + shape.second->getID() + "' in AttributeCarriers destructor");
            delete shape.second;
        }
    }
    // Drop demand elements (Only used for demand elements that were inserted without using GNEChange_DemandElement, for example the default VType")
    for (const auto& demandElementTag : myDemandElements) {
        for (const auto& demandElement : demandElementTag.second) {
            // decrease reference manually (because it was increased manually in GNERouteHandler)
            demandElement.second->decRef();
            // show extra information for tests
            WRITE_DEBUG("Deleting unreferenced " + demandElement.second->getTagStr() + " '" + demandElement.second->getID() + "' in AttributeCarriers destructor");
            delete demandElement.second;
        }
    }
}


void
GNENetHelper::AttributeCarriers::updateID(GNEAttributeCarrier* AC, const std::string newID) {
    if (AC->getTagProperty().getTag() == SUMO_TAG_JUNCTION) {
        updateJunctionID(AC, newID);
    } else if (AC->getTagProperty().getTag() == SUMO_TAG_EDGE) {
        updateEdgeID(AC, newID);
    } else if (AC->getTagProperty().getTag() == SUMO_TAG_TYPE) {
        updateEdgeTypeID(AC, newID);
    } else if (AC->getTagProperty().isAdditionalElement()) {
        updateAdditionalID(AC, newID);
    } else if (AC->getTagProperty().isShape()) {
        updateShapeID(AC, newID);
    } else if (AC->getTagProperty().isTAZElement()) {
        updateTAZElementID(AC, newID);
    } else if (AC->getTagProperty().isDemandElement()) {
        updateDemandElementID(AC, newID);
    } else if (AC->getTagProperty().isDataElement()) {
        updateDataSetID(AC, newID);
    } else {
        throw ProcessError("Unknow Attribute Carrier");
    }
}


void
GNENetHelper::AttributeCarriers::remapJunctionAndEdgeIds() {
    std::map<std::string, GNEEdge*> newEdgeMap;
    std::map<std::string, GNEJunction*> newJunctionMap;
    // fill newEdgeMap
    for (const auto& edge : myEdges) {
        edge.second->setMicrosimID(edge.second->getNBEdge()->getID());
        newEdgeMap[edge.second->getNBEdge()->getID()] = edge.second;
    }
    for (const auto& junction : myJunctions) {
        newJunctionMap[junction.second->getNBNode()->getID()] = junction.second;
        junction.second->setMicrosimID(junction.second->getNBNode()->getID());
    }
    myEdges = newEdgeMap;
    myJunctions = newJunctionMap;
}


GNEJunction*
GNENetHelper::AttributeCarriers::registerJunction(GNEJunction* junction) {
    // increase reference
    junction->incRef("GNENet::registerJunction");
    junction->setResponsible(false);
    myJunctions[junction->getMicrosimID()] = junction;
    // expand net boundary
    myNet->expandBoundary(junction->getCenteringBoundary());
    // add edge into grid
    myNet->addGLObjectIntoGrid(junction);
    // update geometry
    junction->updateGeometry();
    // add z in net boundary
    myNet->addZValueInBoundary(junction->getNBNode()->getPosition().z());
    return junction;
}


const std::map<std::string, GNEJunction*>&
GNENetHelper::AttributeCarriers::getJunctions() const {
    return myJunctions;
}


void
GNENetHelper::AttributeCarriers::clearJunctions() {
    myJunctions.clear();
}


GNEEdgeType*
GNENetHelper::AttributeCarriers::registerEdgeType(GNEEdgeType* edgeType) {
    // increase reference
    edgeType->incRef("GNENet::registerEdgeType");
    // add it in container
    myEdgeTypes[edgeType->getMicrosimID()] = edgeType;
    return edgeType;
}


const std::map<std::string, GNEEdgeType*>&
GNENetHelper::AttributeCarriers::getEdgeTypes() const {
    return myEdgeTypes;
}


void GNENetHelper::AttributeCarriers::clearEdgeTypes() {
    myEdgeTypes.clear();
}


GNEEdge*
GNENetHelper::AttributeCarriers::registerEdge(GNEEdge* edge) {
    edge->incRef("GNENet::registerEdge");
    edge->setResponsible(false);
    // add edge to internal container of GNENet
    myEdges[edge->getMicrosimID()] = edge;
    // expand edge boundary
    myNet->expandBoundary(edge->getCenteringBoundary());
    // add edge into grid
    myNet->addGLObjectIntoGrid(edge);
    // Add references into GNEJunctions
    edge->getParentJunctions().front()->addOutgoingGNEEdge(edge);
    edge->getParentJunctions().back()->addIncomingGNEEdge(edge);
    return edge;
}


const std::map<std::string, GNEEdge*>&
GNENetHelper::AttributeCarriers::getEdges() const {
    return myEdges;
}


void GNENetHelper::AttributeCarriers::clearEdges() {
    myEdges.clear();
}


bool
GNENetHelper::AttributeCarriers::addPolygon(const std::string& id, const std::string& type, const RGBColor& color, double layer, double angle,
        const std::string& imgFile, bool relativePath, const PositionVector& shape, bool geo, bool fill, double lineWidth, bool /*ignorePruning*/) {
    // check if ID is duplicated
    if (myShapes.at(SUMO_TAG_POLY).count(id) == 0) {
        // create poly
        GNEPoly* poly = new GNEPoly(myNet, id, type, shape, geo, fill, lineWidth, color, layer, angle, imgFile, relativePath, false, false);
        if (myAllowUndoShapes) {
            myNet->getViewNet()->getUndoList()->p_begin("add " + toString(SUMO_TAG_POLY));
            myNet->getViewNet()->getUndoList()->add(new GNEChange_Shape(poly, true), true);
            myNet->getViewNet()->getUndoList()->p_end();
        } else {
            // insert shape without allowing undo/redo
            insertShape(poly);
            poly->incRef("addPolygon");
        }
        return true;
    } else {
        return false;
    }
}


bool
GNENetHelper::AttributeCarriers::addPOI(const std::string& id, const std::string& type, const RGBColor& color, const Position& pos, bool geo,
                                        const std::string& lane, double posOverLane, double posLat, double layer, double angle,
                                        const std::string& imgFile, bool relativePath, double width, double height, bool /*ignorePruning*/) {
    // check if ID is duplicated
    if (myShapes.at(SUMO_TAG_POI).count(id) == 0) {
        // create POI or POILane depending of parameter lane
        if (lane.empty()) {
            // create POI
            GNEPOI* POI = new GNEPOI(myNet, id, type, color, pos, geo, layer, angle, imgFile, relativePath, width, height, false);
            if (myAllowUndoShapes) {
                myNet->getViewNet()->getUndoList()->p_begin("add " + POI->getTagStr());
                myNet->getViewNet()->getUndoList()->add(new GNEChange_Shape(POI, true), true);
                myNet->getViewNet()->getUndoList()->p_end();
            } else {
                // insert shape without allowing undo/redo
                insertShape(POI);
                POI->incRef("addPOI");
            }
            return true;
        } else {
            // create POI over lane
            GNELane* retrievedLane = myNet->retrieveLane(lane, false);
            if (retrievedLane == nullptr) {
                WRITE_ERROR("invalid lane to use within POI " + id);
            } else {
                GNEShape* POILane = new GNEPOI(myNet, id, type, color, layer, angle, imgFile, relativePath, retrievedLane, posOverLane, posLat, width, height, false);
                if (myAllowUndoShapes) {
                    myNet->getViewNet()->getUndoList()->p_begin("add " + POILane->getTagStr());
                    myNet->getViewNet()->getUndoList()->add(new GNEChange_Shape(POILane, true), true);
                    myNet->getViewNet()->getUndoList()->p_end();
                } else {
                    // insert shape without allowing undo/redo
                    insertShape(POILane);
                    retrievedLane->addChildElement(POILane);
                    POILane->incRef("addPOILane");
                }
            }
            return true;
        }
    } else {
        return false;
    }
}


const std::map<SumoXMLTag, std::map<std::string, GNEAdditional*> >&
GNENetHelper::AttributeCarriers::getAdditionals() const {
    return myAdditionals;
}


void
GNENetHelper::AttributeCarriers::clearAdditionals() {
    // clear elements in grid
    for (const auto& additionalsTags : myAdditionals) {
        for (const auto& additional : additionalsTags.second) {
            myNet->removeGLObjectFromGrid(additional.second);
        }
    }
    // iterate over myAdditionals and clear all additionals
    for (auto& additionals : myAdditionals) {
        additionals.second.clear();
    }
}


const std::map<SumoXMLTag, std::map<std::string, GNEShape*> >&
GNENetHelper::AttributeCarriers::getShapes() const {
    return myShapes;
}


void
GNENetHelper::AttributeCarriers::clearShapes() {
    // clear elements in grid
    for (const auto& shapesTags : myShapes) {
        for (const auto& shape : shapesTags.second) {
            myNet->removeGLObjectFromGrid(shape.second);
        }
    }
    // iterate over myShapes and clear all shapes
    for (auto& shapes : myShapes) {
        shapes.second.clear();
    }
}


const std::map<SumoXMLTag, std::map<std::string, GNETAZElement*> >&
GNENetHelper::AttributeCarriers::getTAZElements() const {
    return myTAZElements;
}


void
GNENetHelper::AttributeCarriers::clearTAZElements() {
    // clear elements in grid
    for (const auto& TAZElementsTags : myTAZElements) {
        for (const auto& TAZElement : TAZElementsTags.second) {
            myNet->removeGLObjectFromGrid(TAZElement.second);
        }
    }
    // iterate over myTAZElements and clear all TAZElements
    for (auto& TAZElements : myTAZElements) {
        TAZElements.second.clear();
    }
}


const std::map<SumoXMLTag, std::map<std::string, GNEDemandElement*> >&
GNENetHelper::AttributeCarriers::getDemandElements() const {
    return myDemandElements;
}


void
GNENetHelper::AttributeCarriers::clearDemandElements() {
    // clear elements in grid
    for (const auto& demandElementsTags : myDemandElements) {
        for (const auto& demandElement : demandElementsTags.second) {
            myNet->removeGLObjectFromGrid(demandElement.second);
        }
    }
    // iterate over myDemandElements and clear all demand elemnts
    for (auto& demandElements : myDemandElements) {
        demandElements.second.clear();
    }
}


void
GNENetHelper::AttributeCarriers::addDefaultVTypes() {
    // Create default vehicle Type (it has to be created here due myViewNet was previously nullptr)
    GNEVehicleType* defaultVehicleType = new GNEVehicleType(myNet, DEFAULT_VTYPE_ID, SVC_PASSENGER, SUMO_TAG_VTYPE);
    myDemandElements.at(defaultVehicleType->getTagProperty().getTag()).insert(std::make_pair(defaultVehicleType->getID(), defaultVehicleType));
    defaultVehicleType->incRef("GNENet::DEFAULT_VEHTYPE");

    // Create default Bike Type (it has to be created here due myViewNet was previously nullptr)
    GNEVehicleType* defaultBikeType = new GNEVehicleType(myNet, DEFAULT_BIKETYPE_ID, SVC_BICYCLE, SUMO_TAG_VTYPE);
    myDemandElements.at(defaultBikeType->getTagProperty().getTag()).insert(std::make_pair(defaultBikeType->getID(), defaultBikeType));
    defaultBikeType->incRef("GNENet::DEFAULT_BIKETYPE_ID");

    // Create default person Type (it has to be created here due myViewNet was previously nullptr)
    GNEVehicleType* defaultPersonType = new GNEVehicleType(myNet, DEFAULT_PEDTYPE_ID, SVC_PEDESTRIAN, SUMO_TAG_PTYPE);
    myDemandElements.at(defaultPersonType->getTagProperty().getTag()).insert(std::make_pair(defaultPersonType->getID(), defaultPersonType));
    defaultPersonType->incRef("GNENet::DEFAULT_PEDTYPE_ID");
}


const std::map<std::string, GNEDataSet*>&
GNENetHelper::AttributeCarriers::getDataSets() const {
    return myDataSets;
}


void
GNENetHelper::AttributeCarriers::clearDataSets() {
    // clear elements in grid
    for (const auto& dataSet : myDataSets) {
        for (const auto& dataInterval : dataSet.second->getDataIntervalChildren()) {
            for (const auto& genericData : dataInterval.second->getGenericDataChildren()) {
                myNet->removeGLObjectFromGrid(genericData);
            }
        }
    }
    // just clear myDataSets
    myDataSets.clear();
}


std::vector<GNEGenericData*>
GNENetHelper::AttributeCarriers::retrieveGenericDatas(const SumoXMLTag genericDataTag, const double begin, const double end) {
    // declare generic data vector
    std::vector<GNEGenericData*> genericDatas;
    // iterate over all data sets
    for (const auto& dataSet : myDataSets) {
        for (const auto& interval : dataSet.second->getDataIntervalChildren()) {
            // check interval
            if ((interval.second->getAttributeDouble(SUMO_ATTR_BEGIN) >= begin) && (interval.second->getAttributeDouble(SUMO_ATTR_END) <= end)) {
                // iterate over generic datas
                for (const auto& genericData : interval.second->getGenericDataChildren()) {
                    if (genericData->getTagProperty().getTag() == genericDataTag) {
                        genericDatas.push_back(genericData);
                    }
                }
            }
        }
    }
    return genericDatas;
}


void
GNENetHelper::AttributeCarriers::insertJunction(GNEJunction* junction) {
    myNet->getNetBuilder()->getNodeCont().insert(junction->getNBNode());
    registerJunction(junction);
}


void
GNENetHelper::AttributeCarriers::deleteSingleJunction(GNEJunction* junction) {
    // remove it from inspected elements and HierarchicalElementTree
    myNet->getViewNet()->removeFromAttributeCarrierInspected(junction);
    myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(junction);
    // Remove from grid and container
    myNet->removeGLObjectFromGrid(junction);
    myJunctions.erase(junction->getMicrosimID());
    myNet->getNetBuilder()->getNodeCont().extract(junction->getNBNode());
    junction->decRef("GNENet::deleteSingleJunction");
    junction->setResponsible(true);
}


void
GNENetHelper::AttributeCarriers::updateJunctionID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myJunctions.count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.junction");
    } else if (myJunctions.count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in myJunctions");
    } else {
        // retrieve junction
        GNEJunction* junction = myJunctions.at(AC->getID());
        // remove junction from container
        myJunctions.erase(junction->getNBNode()->getID());
        // rename in NetBuilder
        myNet->getNetBuilder()->getNodeCont().rename(junction->getNBNode(), newID);
        // update microsim ID
        junction->setMicrosimID(newID);
        // add it into myJunctions again
        myJunctions[AC->getID()] = junction;
        // build crossings
        junction->getNBNode()->buildCrossings();
        // net has to be saved
        myNet->requireSaveNet(true);
    }
}


bool
GNENetHelper::AttributeCarriers::edgeTypeExist(const GNEEdgeType* edgeType) const {
    return (myEdgeTypes.count(edgeType->getID()) > 0);
}


void
GNENetHelper::AttributeCarriers::insertEdgeType(GNEEdgeType* edgeType) {
    // insert in myEdgeTypes
    myEdgeTypes[edgeType->getMicrosimID()] = edgeType;
    // update edge selector
    if (myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->shown()) {
        myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->getEdgeTypeSelector()->refreshEdgeTypeSelector();
    }
}


void
GNENetHelper::AttributeCarriers::deleteEdgeType(GNEEdgeType* edgeType) {
    // remove it from inspected elements and HierarchicalElementTree
    myNet->getViewNet()->removeFromAttributeCarrierInspected(edgeType);
    myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(edgeType);
    // remove from edge types
    myEdgeTypes.erase(edgeType->getMicrosimID());
    // check if this is the selected edge type in edgeSelector
    if (myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->getEdgeTypeSelector()->getEdgeTypeSelected() == edgeType) {
        myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->getEdgeTypeSelector()->clearEdgeTypeSelected();
        myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->getEdgeTypeSelector()->refreshEdgeTypeSelector();
    }
    // update edge selector
    if (myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->shown()) {
        myNet->getViewNet()->getViewParent()->getCreateEdgeFrame()->getEdgeTypeSelector()->refreshEdgeTypeSelector();
    }
}


void
GNENetHelper::AttributeCarriers::updateEdgeTypeID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myEdgeTypes.count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.edgeType");
    } else if (myEdgeTypes.count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in myEdgeTypes");
    } else {
        // retrieve edgeType
        GNEEdgeType* edgeType = myEdgeTypes.at(AC->getID());
        // remove edgeType from container
        myEdgeTypes.erase(edgeType->getID());
        // rename in typeCont
        myNet->getNetBuilder()->getTypeCont().updateEdgeTypeID(edgeType->getID(), newID);
        // update microsim ID
        edgeType->setMicrosimID(newID);
        // add it into myEdgeTypes again
        myEdgeTypes[AC->getID()] = edgeType;
        // net has to be saved
        myNet->requireSaveNet(true);
    }
}


void
GNENetHelper::AttributeCarriers::insertEdge(GNEEdge* edge) {
    NBEdge* nbe = edge->getNBEdge();
    myNet->getNetBuilder()->getEdgeCont().insert(nbe); // should we ignore pruning double edges?
    // if this edge was previouls extracted from the edgeContainer we have to rewire the nodes
    nbe->getFromNode()->addOutgoingEdge(nbe);
    nbe->getToNode()->addIncomingEdge(nbe);
    registerEdge(edge);
}


void
GNENetHelper::AttributeCarriers::deleteSingleEdge(GNEEdge* edge) {
    // remove it from inspected elements and HierarchicalElementTree
    myNet->getViewNet()->removeFromAttributeCarrierInspected(edge);
    myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(edge);
    // remove edge from visual grid and container
    myNet->removeGLObjectFromGrid(edge);
    myEdges.erase(edge->getMicrosimID());
    // extract edge of district container
    myNet->getNetBuilder()->getEdgeCont().extract(myNet->getNetBuilder()->getDistrictCont(), edge->getNBEdge());
    edge->decRef("GNENet::deleteSingleEdge");
    edge->setResponsible(true);
    // Remove refrences from GNEJunctions
    edge->getParentJunctions().front()->removeOutgoingGNEEdge(edge);
    edge->getParentJunctions().back()->removeIncomingGNEEdge(edge);
}


void
GNENetHelper::AttributeCarriers::updateEdgeID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myEdges.count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.edge");
    } else if (myEdges.count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in myEdges");
    } else {
        // retrieve edge
        GNEEdge* edge = myEdges.at(AC->getID());
        // remove edge from container
        myEdges.erase(edge->getNBEdge()->getID());
        // rename in NetBuilder
        myNet->getNetBuilder()->getEdgeCont().rename(edge->getNBEdge(), newID);
        // update microsim ID
        edge->setMicrosimID(newID);
        // add it into myEdges again
        myEdges[AC->getID()] = edge;
        // rename all connections related to this edge
        for (const auto& lane : edge->getLanes()) {
            lane->updateConnectionIDs();
        }
        // net has to be saved
        myNet->requireSaveNet(true);
    }
}


bool
GNENetHelper::AttributeCarriers::additionalExist(const GNEAdditional* additional) const {
    // first check that additional pointer is valid
    if (additional) {
        return myAdditionals.at(additional->getTagProperty().getTag()).find(additional->getID()) !=
               myAdditionals.at(additional->getTagProperty().getTag()).end();
    } else {
        throw ProcessError("Invalid additional pointer");
    }
}


void
GNENetHelper::AttributeCarriers::insertAdditional(GNEAdditional* additional) {
    // check if additional is a slave
    if (!additional->getTagProperty().isSlave()) {
        // check if previously was inserted
        if (!additionalExist(additional)) {
            // insert additional
            myAdditionals.at(additional->getTagProperty().getTag()).insert(std::make_pair(additional->getID(), additional));
        } else {
            throw ProcessError(additional->getTagStr() + " with ID='" + additional->getID() + "' already exist");
        }
    }
    // add element in grid
    if (additional->getTagProperty().isPlacedInRTree()) {
        myNet->addGLObjectIntoGrid(additional);
    }
    // update geometry after insertion of additionals if myUpdateGeometryEnabled is enabled
    if (myNet->isUpdateGeometryEnabled()) {
        additional->updateGeometry();
    }
    // additionals has to be saved
    myNet->requireSaveAdditionals(true);
}


void
GNENetHelper::AttributeCarriers::deleteAdditional(GNEAdditional* additional) {
    // check if additional is a slave
    if (!additional->getTagProperty().isSlave()) {
        // check if previously was inserted
        if (additionalExist(additional)) {
            // remove it from inspected elements and HierarchicalElementTree
            myNet->getViewNet()->removeFromAttributeCarrierInspected(additional);
            myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(additional);
            // obtain additional iterator and erase it from container
            auto it = myAdditionals.at(additional->getTagProperty().getTag()).find(additional->getID());
            myAdditionals.at(additional->getTagProperty().getTag()).erase(it);
        } else {
            throw ProcessError("Invalid additional pointer");
        }
    }
    // remove element from grid
    if (additional->getTagProperty().isPlacedInRTree()) {
        myNet->removeGLObjectFromGrid(additional);
    }
    // delete path element
    myNet->getPathManager()->removePath(additional);
    // additionals has to be saved
    myNet->requireSaveAdditionals(true);
}


void
GNENetHelper::AttributeCarriers::updateAdditionalID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myAdditionals.at(AC->getTagProperty().getTag()).count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.additionals");
    } else if (myAdditionals.at(AC->getTagProperty().getTag()).count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in AttributeCarriers.additionals");
    } else {
        // retrieve additional
        GNEAdditional* additional = myAdditionals.at(AC->getTagProperty().getTag()).at(AC->getID());
        // remove additional from container
        myAdditionals.at(additional->getTagProperty().getTag()).erase(additional->getID());
        // set new ID in additional
        additional->setMicrosimID(newID);
        // insert additional again in container
        myAdditionals.at(additional->getTagProperty().getTag()).insert(std::make_pair(additional->getID(), additional));
        // additionals has to be saved
        myNet->requireSaveAdditionals(true);
    }
}


bool
GNENetHelper::AttributeCarriers::shapeExist(const GNEShape* shape) const {
    // get tag (due POIs)
    SumoXMLTag shapeTag = (shape->getTagProperty().getTag() == SUMO_TAG_POILANE) ? SUMO_TAG_POI : shape->getTagProperty().getTag();
    // first check that shape pointer is valid
    if (shape) {
        return myShapes.at(shapeTag).find(shape->getID()) != myShapes.at(shapeTag).end();
    } else {
        throw ProcessError("Invalid shape pointer");
    }
}


void
GNENetHelper::AttributeCarriers::insertShape(GNEShape* shape) {
    // get tag (due POIs)
    SumoXMLTag shapeTag = (shape->getTagProperty().getTag() == SUMO_TAG_POILANE) ? SUMO_TAG_POI : shape->getTagProperty().getTag();
    // Check if shape element exists before insertion
    if (!shapeExist(shape)) {
        myShapes.at(shapeTag).insert(std::make_pair(shape->getID(), shape));
        // add element in grid
        myNet->addGLObjectIntoGrid(shape);
        // update geometry after insertion of shapes if myUpdateGeometryEnabled is enabled
        if (myNet->isUpdateGeometryEnabled()) {
            shape->updateGeometry();
        }
        // shapes has to be saved
        myNet->requireSaveAdditionals(true);
    } else {
        throw ProcessError(shape->getTagStr() + " with ID='" + shape->getID() + "' already exist");
    }
}


void
GNENetHelper::AttributeCarriers::deleteShape(GNEShape* shape) {
    // get tag (due POIs)
    SumoXMLTag shapeTag = (shape->getTagProperty().getTag() == SUMO_TAG_POILANE) ? SUMO_TAG_POI : shape->getTagProperty().getTag();
    // first check that shape pointer is valid
    if (shapeExist(shape)) {
        // remove it from inspected elements and HierarchicalElementTree
        myNet->getViewNet()->removeFromAttributeCarrierInspected(shape);
        myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(shape);
        // obtain demand element and erase it from container
        auto it = myShapes.at(shapeTag).find(shape->getID());
        myShapes.at(shapeTag).erase(it);
        // remove element from grid
        myNet->removeGLObjectFromGrid(shape);
        // shapes has to be saved
        myNet->requireSaveAdditionals(true);
    } else {
        throw ProcessError("Invalid shape pointer");
    }
}


void
GNENetHelper::AttributeCarriers::updateShapeID(GNEAttributeCarrier* AC, const std::string& newID) {
    // get tag (due POIs)
    SumoXMLTag shapeTag = (AC->getTagProperty().getTag() == SUMO_TAG_POILANE) ? SUMO_TAG_POI : AC->getTagProperty().getTag();
    // continue
    if (myShapes.at(shapeTag).count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.shapes");
    } else if (myShapes.at(shapeTag).count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in AttributeCarriers.shapes");
    } else {
        // retrieve shape
        GNEShape* shape = myShapes.at(shapeTag).at(AC->getID());
        // remove shape from container
        myShapes.at(shapeTag).erase(shape->getID());
        // set new ID in shape
        shape->getGUIGlObject()->setMicrosimID(newID);
        // insert shape again in container
        myShapes.at(shapeTag).insert(std::make_pair(shape->getID(), shape));
        // shapes has to be saved
        myNet->requireSaveAdditionals(true);
    }
}


bool
GNENetHelper::AttributeCarriers::TAZElementExist(const GNETAZElement* TAZElement) const {
    // first check that TAZElement pointer is valid
    if (TAZElement) {
        return myTAZElements.at(TAZElement->getTagProperty().getTag()).find(TAZElement->getID()) !=
               myTAZElements.at(TAZElement->getTagProperty().getTag()).end();
    } else {
        throw ProcessError("Invalid TAZElement pointer");
    }
}


void
GNENetHelper::AttributeCarriers::insertTAZElement(GNETAZElement* TAZElement) {
    // Check if TAZElement element exists before insertion
    if (!TAZElementExist(TAZElement)) {
        myTAZElements.at(TAZElement->getTagProperty().getTag()).insert(std::make_pair(TAZElement->getID(), TAZElement));
        // add element in grid
        myNet->addGLObjectIntoGrid(TAZElement);
        // update geometry after insertion of TAZElements if myUpdateGeometryEnabled is enabled
        if (myNet->isUpdateGeometryEnabled()) {
            TAZElement->updateGeometry();
        }
        // TAZElements has to be saved
        myNet->requireSaveAdditionals(true);
    } else {
        throw ProcessError(TAZElement->getTagStr() + " with ID='" + TAZElement->getID() + "' already exist");
    }
}


void
GNENetHelper::AttributeCarriers::deleteTAZElement(GNETAZElement* TAZElement) {
    // first check that TAZElement pointer is valid
    if (TAZElementExist(TAZElement)) {
        // remove it from inspected elements and HierarchicalElementTree
        myNet->getViewNet()->removeFromAttributeCarrierInspected(TAZElement);
        myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(TAZElement);
        // obtain demand element and erase it from container
        auto it = myTAZElements.at(TAZElement->getTagProperty().getTag()).find(TAZElement->getID());
        myTAZElements.at(TAZElement->getTagProperty().getTag()).erase(it);
        // remove element from grid
        myNet->removeGLObjectFromGrid(TAZElement);
        // TAZElements has to be saved
        myNet->requireSaveAdditionals(true);
    } else {
        throw ProcessError("Invalid TAZElement pointer");
    }
}


void
GNENetHelper::AttributeCarriers::updateTAZElementID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myTAZElements.at(AC->getTagProperty().getTag()).count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.TAZElements");
    } else if (myTAZElements.at(AC->getTagProperty().getTag()).count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in AttributeCarriers.TAZElements");
    } else {
        // retrieve TAZElement
        GNETAZElement* TAZElement = myTAZElements.at(AC->getTagProperty().getTag()).at(AC->getID());
        // remove TAZElement from container
        myTAZElements.at(TAZElement->getTagProperty().getTag()).erase(TAZElement->getID());
        // set new ID in TAZElement
        TAZElement->getGUIGlObject()->setMicrosimID(newID);
        // insert TAZElement again in container
        myTAZElements.at(TAZElement->getTagProperty().getTag()).insert(std::make_pair(TAZElement->getID(), TAZElement));
        // TAZElements has to be saved
        myNet->requireSaveAdditionals(true);
    }
}


bool
GNENetHelper::AttributeCarriers::demandElementExist(const GNEDemandElement* demandElement) const {
    // first check that demandElement pointer is valid
    if (demandElement) {
        return myDemandElements.at(demandElement->getTagProperty().getTag()).find(demandElement->getID()) !=
               myDemandElements.at(demandElement->getTagProperty().getTag()).end();
    } else {
        throw ProcessError("Invalid demandElement pointer");
    }
}


void
GNENetHelper::AttributeCarriers::insertDemandElement(GNEDemandElement* demandElement) {
    // check if demandElement is a slave
    if (!demandElement->getTagProperty().isSlave()) {
        // Check if demandElement element exists before insertion
        if (!demandElementExist(demandElement)) {
            // insert in demandElements container
            myDemandElements.at(demandElement->getTagProperty().getTag()).insert(std::make_pair(demandElement->getID(), demandElement));
        } else {
            throw ProcessError(demandElement->getTagStr() + " with ID='" + demandElement->getID() + "' already exist");
        }
    }
    // add element in grid
    myNet->addGLObjectIntoGrid(demandElement);
    // update geometry after insertion of demandElements if myUpdateGeometryEnabled is enabled
    if (myNet->isUpdateGeometryEnabled()) {
        demandElement->updateGeometry();
    }
    // demandElements has to be saved
    myNet->requireSaveDemandElements(true);
}


void
GNENetHelper::AttributeCarriers::deleteDemandElement(GNEDemandElement* demandElement) {
    // check if demandElement is a slave
    if (!demandElement->getTagProperty().isSlave()) {
        // first check that demandElement pointer is valid
        if (demandElementExist(demandElement)) {
            // remove it from inspected elements and HierarchicalElementTree
            myNet->getViewNet()->removeFromAttributeCarrierInspected(demandElement);
            myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(demandElement);
            // obtain demand element and erase it from container
            auto it = myDemandElements.at(demandElement->getTagProperty().getTag()).find(demandElement->getID());
            myDemandElements.at(demandElement->getTagProperty().getTag()).erase(it);
        } else {
            throw ProcessError("Invalid demandElement pointer");
        }
    }
    // remove element from grid
    myNet->removeGLObjectFromGrid(demandElement);
    // delete path element
    myNet->getPathManager()->removePath(demandElement);
    // demandElements has to be saved
    myNet->requireSaveDemandElements(true);
}


void
GNENetHelper::AttributeCarriers::updateDemandElementID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myDemandElements.at(AC->getTagProperty().getTag()).count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.myDemandElements");
    } else if (myDemandElements.at(AC->getTagProperty().getTag()).count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in AttributeCarriers.myDemandElements");
    } else {
        // retrieve demand element
        GNEDemandElement* demandElement = myDemandElements.at(AC->getTagProperty().getTag()).at(AC->getID());
        // get embebbed route
        GNEDemandElement* embebbedRoute = nullptr;
        if (demandElement->getTagProperty().embebbedRoute()) {
            embebbedRoute = demandElement->getChildDemandElements().back();
        }
        // remove demand element from container
        myDemandElements.at(demandElement->getTagProperty().getTag()).erase(demandElement->getID());
        // remove embebbed route from container
        if (embebbedRoute) {
            myDemandElements.at(GNE_TAG_ROUTE_EMBEDDED).erase(embebbedRoute->getID());
        }
        // set new ID in demand
        demandElement->setMicrosimID(newID);
        // insert demand again in container
        myDemandElements.at(demandElement->getTagProperty().getTag()).insert(std::make_pair(demandElement->getID(), demandElement));
        // insert emebbed route again in container
        if (embebbedRoute) {
            // set new microsim ID
            embebbedRoute->setMicrosimID(embebbedRoute->getID());
            myDemandElements.at(GNE_TAG_ROUTE_EMBEDDED).insert(std::make_pair(embebbedRoute->getID(), embebbedRoute));
        }
        // myDemandElements has to be saved
        myNet->requireSaveDemandElements(true);
    }
}


bool
GNENetHelper::AttributeCarriers::dataSetExist(const GNEDataSet* dataSet) const {
    // first check that dataSet pointer is valid
    if (dataSet) {
        for (const auto& dataset : myDataSets) {
            if (dataset.second == dataSet) {
                return true;
            }
        }
        return false;
    } else {
        throw ProcessError("Invalid dataSet pointer");
    }
}


void
GNENetHelper::AttributeCarriers::insertDataSet(GNEDataSet* dataSet) {
    // Check if dataSet element exists before insertion
    if (!dataSetExist(dataSet)) {
        // insert in dataSets container
        myDataSets.insert(std::make_pair(dataSet->getID(), dataSet));
        // dataSets has to be saved
        myNet->requireSaveDataElements(true);
        // update interval toolbar
        myNet->getViewNet()->getIntervalBar().updateIntervalBar();
    } else {
        throw ProcessError(dataSet->getTagStr() + " with ID='" + dataSet->getID() + "' already exist");
    }
}


void
GNENetHelper::AttributeCarriers::deleteDataSet(GNEDataSet* dataSet) {
    // first check that dataSet pointer is valid
    if (dataSetExist(dataSet)) {
        // remove it from inspected elements and HierarchicalElementTree
        myNet->getViewNet()->removeFromAttributeCarrierInspected(dataSet);
        myNet->getViewNet()->getViewParent()->getInspectorFrame()->getHierarchicalElementTree()->removeCurrentEditedAttributeCarrier(dataSet);
        // obtain demand element and erase it from container
        myDataSets.erase(myDataSets.find(dataSet->getID()));
        // dataSets has to be saved
        myNet->requireSaveDataElements(true);
        // update interval toolbar
        myNet->getViewNet()->getIntervalBar().updateIntervalBar();
    } else {
        throw ProcessError("Invalid dataSet pointer");
    }
}


void
GNENetHelper::AttributeCarriers::updateDataSetID(GNEAttributeCarrier* AC, const std::string& newID) {
    if (myDataSets.count(AC->getID()) == 0) {
        throw ProcessError(AC->getTagStr() + " with ID='" + AC->getID() + "' doesn't exist in AttributeCarriers.dataSets");
    } else if (myDataSets.count(newID) != 0) {
        throw ProcessError("There is another " + AC->getTagStr() + " with new ID='" + newID + "' in dataSets");
    } else {
        // retrieve dataSet
        GNEDataSet* dataSet = myDataSets.at(AC->getID());
        // remove dataSet from container
        myDataSets.erase(dataSet->getID());
        // set new ID in dataSet
        dataSet->setDataSetID(newID);
        // insert dataSet again in container
        myDataSets[dataSet->getID()] = dataSet;
        // data sets has to be saved
        myNet->requireSaveDataElements(true);
        // update interval toolbar
        myNet->getViewNet()->getIntervalBar().updateIntervalBar();
    }
}

// ---------------------------------------------------------------------------
// GNENetHelper::GNEChange_ReplaceEdgeInTLS - methods
// ---------------------------------------------------------------------------

GNENetHelper::GNEChange_ReplaceEdgeInTLS::GNEChange_ReplaceEdgeInTLS(NBTrafficLightLogicCont& tllcont, NBEdge* replaced, NBEdge* by) :
    GNEChange(true, false),
    myTllcont(tllcont),
    myReplaced(replaced),
    myBy(by) {
}


GNENetHelper::GNEChange_ReplaceEdgeInTLS::~GNEChange_ReplaceEdgeInTLS() {}


FXString
GNENetHelper::GNEChange_ReplaceEdgeInTLS::undoName() const {
    return "Redo replace in TLS";
}


FXString
GNENetHelper::GNEChange_ReplaceEdgeInTLS::redoName() const {
    return "Undo replace in TLS";
}


void
GNENetHelper::GNEChange_ReplaceEdgeInTLS::undo() {
    // assuming this is only used for replacing incoming connections (GNENet::replaceIncomingEdge)
    myTllcont.replaceRemoved(myBy, -1, myReplaced, -1, true);
}


void
GNENetHelper::GNEChange_ReplaceEdgeInTLS::redo() {
    // assuming this is only used for replacing incoming connections (GNENet::replaceIncomingEdge)
    myTllcont.replaceRemoved(myReplaced, -1, myBy, -1, true);
}


bool
GNENetHelper::GNEChange_ReplaceEdgeInTLS::trueChange() {
    return myReplaced != myBy;
}

/****************************************************************************/
