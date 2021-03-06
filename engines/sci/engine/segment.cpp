/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#include "common/endian.h"

#include "sci/sci.h"
#include "sci/engine/kernel.h"
#include "sci/engine/features.h"
#include "sci/engine/object.h"
#include "sci/engine/script.h"	// for SCI_OBJ_EXPORTS and SCI_OBJ_SYNONYMS
#include "sci/engine/segment.h"
#include "sci/engine/seg_manager.h"
#include "sci/engine/state.h"

namespace Sci {

//#define GC_DEBUG // Debug garbage collection
//#define GC_DEBUG_VERBOSE // Debug garbage verbosely


SegmentObj *SegmentObj::createSegmentObj(SegmentType type) {
	SegmentObj *mem = 0;
	switch (type) {
	case SEG_TYPE_SCRIPT:
		mem = new Script();
		break;
	case SEG_TYPE_CLONES:
		mem = new CloneTable();
		break;
	case SEG_TYPE_LOCALS:
		mem = new LocalVariables();
		break;
	case SEG_TYPE_STACK:
		mem = new DataStack();
		break;
	case SEG_TYPE_HUNK:
		mem = new HunkTable();
		break;
	case SEG_TYPE_LISTS:
		mem = new ListTable();
		break;
	case SEG_TYPE_NODES:
		mem = new NodeTable();
		break;
	case SEG_TYPE_DYNMEM:
		mem = new DynMem();
		break;
#ifdef ENABLE_SCI32
	case SEG_TYPE_ARRAY:
		mem = new ArrayTable();
		break;
	case SEG_TYPE_STRING:
		mem = new StringTable();
		break;
#endif
	default:
		error("Unknown SegmentObj type %d", type);
		break;
	}

	assert(mem);
	assert(mem->_type == type);
	return mem;
}

const char *SegmentObj::getSegmentTypeName(SegmentType type) {
	switch (type) {
	case SEG_TYPE_SCRIPT:
		return "script";
		break;
	case SEG_TYPE_CLONES:
		return "clones";
		break;
	case SEG_TYPE_LOCALS:
		return "locals";
		break;
	case SEG_TYPE_STACK:
		return "stack";
		break;
	case SEG_TYPE_HUNK:
		return "hunk";
		break;
	case SEG_TYPE_LISTS:
		return "lists";
		break;
	case SEG_TYPE_NODES:
		return "nodes";
		break;
	case SEG_TYPE_DYNMEM:
		return "dynmem";
		break;
#ifdef ENABLE_SCI32
	case SEG_TYPE_ARRAY:
		return "array";
		break;
	case SEG_TYPE_STRING:
		return "string";
		break;
#endif
	default:
		error("Unknown SegmentObj type %d", type);
		break;
	}
	return NULL;
}

SegmentRef SegmentObj::dereference(reg_t pointer) {
	error("Error: Trying to dereference pointer %04x:%04x to inappropriate segment",
		          PRINT_REG(pointer));
	return SegmentRef();
}


bool LocalVariables::isValidOffset(uint16 offset) const {
	return offset < _locals.size() * 2;
}

SegmentRef LocalVariables::dereference(reg_t pointer) {
	SegmentRef ret;
	ret.isRaw = false;	// reg_t based data!
	ret.maxSize = (_locals.size() - pointer.offset / 2) * 2;

	if (pointer.offset & 1) {
		ret.maxSize -= 1;
		ret.skipByte = true;
	}

	if (ret.maxSize > 0) {
		ret.reg = &_locals[pointer.offset / 2];
	} else {
		if ((g_sci->getEngineState()->currentRoomNumber() == 660 || g_sci->getEngineState()->currentRoomNumber() == 660)
			&& g_sci->getGameId() == GID_LAURABOW2) {
			// Happens in two places during the intro of LB2CD, both from kMemory(peek):
			// - room 160: Heap 160 has 83 local variables (0-82), and the game
			//   asks for variables at indices 83 - 90 too.
			// - room 220: Heap 220 has 114 local variables (0-113), and the
			//   game asks for variables at indices 114-120 too.
		} else {
			error("LocalVariables::dereference: Offset at end or out of bounds %04x:%04x", PRINT_REG(pointer));
		}
		ret.reg = 0;
	}
	return ret;
}

bool DataStack::isValidOffset(uint16 offset) const {
	return offset < _capacity * 2;
}

SegmentRef DataStack::dereference(reg_t pointer) {
	SegmentRef ret;
	ret.isRaw = false;	// reg_t based data!
	ret.maxSize = (_capacity - pointer.offset / 2) * 2;

	if (pointer.offset & 1) {
		ret.maxSize -= 1;
		ret.skipByte = true;
	}

	ret.reg = &_entries[pointer.offset / 2];
	return ret;
}

bool DynMem::isValidOffset(uint16 offset) const {
	return offset < _size;
}

SegmentRef DynMem::dereference(reg_t pointer) {
	SegmentRef ret;
	ret.isRaw = true;
	ret.maxSize = _size - pointer.offset;
	ret.raw = _buf + pointer.offset;
	return ret;
}

//-------------------- clones --------------------

Common::Array<reg_t> CloneTable::listAllOutgoingReferences(reg_t addr) const {
	Common::Array<reg_t> tmp;
//	assert(addr.segment == _segId);

	if (!isValidEntry(addr.offset)) {
		error("Unexpected request for outgoing references from clone at %04x:%04x", PRINT_REG(addr));
	}

	const Clone *clone = &(_table[addr.offset]);

	// Emit all member variables (including references to the 'super' delegate)
	for (uint i = 0; i < clone->getVarCount(); i++)
		tmp.push_back(clone->getVariable(i));

	// Note that this also includes the 'base' object, which is part of the script and therefore also emits the locals.
	tmp.push_back(clone->getPos());
	//debugC(kDebugLevelGC, "[GC] Reporting clone-pos %04x:%04x", PRINT_REG(clone->pos));

	return tmp;
}

void CloneTable::freeAtAddress(SegManager *segMan, reg_t addr) {
#ifdef GC_DEBUG
	//	assert(addr.segment == _segId);

	Object *victim_obj = &(_table[addr.offset]);

	if (!(victim_obj->_flags & OBJECT_FLAG_FREED))
		warning("[GC] Clone %04x:%04x not reachable and not freed (freeing now)", PRINT_REG(addr));
#ifdef GC_DEBUG_VERBOSE
	else
		warning("[GC-DEBUG] Clone %04x:%04x: Freeing", PRINT_REG(addr));
#endif
#endif
	/*
		warning("[GC] Clone %04x:%04x: Freeing", PRINT_REG(addr));
		warning("[GC] Clone had pos %04x:%04x", PRINT_REG(victim_obj->pos));
	*/
	freeEntry(addr.offset);
}


//-------------------- locals --------------------
reg_t LocalVariables::findCanonicAddress(SegManager *segMan, reg_t addr) const {
	// Reference the owning script
	SegmentId owner_seg = segMan->getScriptSegment(script_id);

	assert(owner_seg > 0);

	return make_reg(owner_seg, 0);
}

Common::Array<reg_t> LocalVariables::listAllOutgoingReferences(reg_t addr) const {
	Common::Array<reg_t> tmp;
//	assert(addr.segment == _segId);

	for (uint i = 0; i < _locals.size(); i++)
		tmp.push_back(_locals[i]);

	return tmp;
}


//-------------------- stack --------------------
reg_t DataStack::findCanonicAddress(SegManager *segMan, reg_t addr) const {
	addr.offset = 0;
	return addr;
}

Common::Array<reg_t> DataStack::listAllOutgoingReferences(reg_t object) const {
	Common::Array<reg_t> tmp;
	for (int i = 0; i < _capacity; i++)
		tmp.push_back(_entries[i]);

	return tmp;
}


//-------------------- hunk ---------------------
void HunkTable::freeAtAddress(SegManager *segMan, reg_t sub_addr) {
	freeEntry(sub_addr.offset);
}

//-------------------- lists --------------------
void ListTable::freeAtAddress(SegManager *segMan, reg_t sub_addr) {
	freeEntry(sub_addr.offset);
}

Common::Array<reg_t> ListTable::listAllOutgoingReferences(reg_t addr) const {
	Common::Array<reg_t> tmp;
	if (!isValidEntry(addr.offset)) {
		error("Invalid list referenced for outgoing references: %04x:%04x", PRINT_REG(addr));
	}

	const List *list = &(_table[addr.offset]);

	tmp.push_back(list->first);
	tmp.push_back(list->last);
	// We could probably get away with just one of them, but
	// let's be conservative here.

	return tmp;
}


//-------------------- nodes --------------------
void NodeTable::freeAtAddress(SegManager *segMan, reg_t sub_addr) {
	freeEntry(sub_addr.offset);
}

Common::Array<reg_t> NodeTable::listAllOutgoingReferences(reg_t addr) const {
	Common::Array<reg_t> tmp;
	if (!isValidEntry(addr.offset)) {
		error("Invalid node referenced for outgoing references: %04x:%04x", PRINT_REG(addr));
	}
	const Node *node = &(_table[addr.offset]);

	// We need all four here. Can't just stick with 'pred' OR 'succ' because node operations allow us
	// to walk around from any given node
	tmp.push_back(node->pred);
	tmp.push_back(node->succ);
	tmp.push_back(node->key);
	tmp.push_back(node->value);

	return tmp;
}

//-------------------- dynamic memory --------------------

reg_t DynMem::findCanonicAddress(SegManager *segMan, reg_t addr) const {
	addr.offset = 0;
	return addr;
}

Common::Array<reg_t> DynMem::listAllDeallocatable(SegmentId segId) const {
	const reg_t r = make_reg(segId, 0);
	return Common::Array<reg_t>(&r, 1);
}

#ifdef ENABLE_SCI32

SegmentRef ArrayTable::dereference(reg_t pointer) {
	SegmentRef ret;
	ret.isRaw = false;
	ret.maxSize = _table[pointer.offset].getSize() * 2;
	ret.reg = _table[pointer.offset].getRawData();
	return ret;
}

void ArrayTable::freeAtAddress(SegManager *segMan, reg_t sub_addr) { 
	_table[sub_addr.offset].destroy();
	freeEntry(sub_addr.offset);
}

Common::Array<reg_t> ArrayTable::listAllOutgoingReferences(reg_t addr) const {
	Common::Array<reg_t> tmp;
	if (!isValidEntry(addr.offset)) {
		error("Invalid array referenced for outgoing references: %04x:%04x", PRINT_REG(addr));
	}

	const SciArray<reg_t> *array = &(_table[addr.offset]);

	for (uint32 i = 0; i < array->getSize(); i++) {
		reg_t value = array->getValue(i);
		if (value.segment != 0)
			tmp.push_back(value);
	}

	return tmp;
}

Common::String SciString::toString() const {
	if (_type != 3)
		error("SciString::toString(): Array is not a string");

	Common::String string;
	for (uint32 i = 0; i < _size && _data[i] != 0; i++)
		string += _data[i];

	return string;
}

void SciString::fromString(const Common::String &string) {
	if (_type != 3)
		error("SciString::fromString(): Array is not a string");

	if (string.size() > _size)
		setSize(string.size());

	for (uint32 i = 0; i < string.size(); i++)
		_data[i] = string[i];
}

SegmentRef StringTable::dereference(reg_t pointer) {
	SegmentRef ret;
	ret.isRaw = true;
	ret.maxSize = _table[pointer.offset].getSize();
	ret.raw = (byte*)_table[pointer.offset].getRawData();
	return ret;
}

void StringTable::freeAtAddress(SegManager *segMan, reg_t sub_addr) { 
	_table[sub_addr.offset].destroy();
	freeEntry(sub_addr.offset);
}

#endif

} // End of namespace Sci
