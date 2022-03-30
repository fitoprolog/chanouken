/**
 * @file lloctree.h
 * @brief Octree declaration.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLOCTREE_H
#define LL_LLOCTREE_H

#include <vector>


#include "llpointer.h"
#include "llrefcount.h"
#include "llvector3.h"
#include "llxform.h"

#define LL_OCTREE_PARANOIA_CHECK 0
#define NO_CHILD_NODES 255

extern U32 gOctreeMaxCapacity;
extern F32 gOctreeMinSize;
extern LLVector4a gOctreeMaxMag;

template <class T> class LLOctreeNode;
template <class T> class LLTreeNode;
template <class T> class LLTreeTraveler;
template <class T> class LLTreeListener;

template <class T>
class LLTreeListener: public LLRefCount
{
public:
	virtual void handleInsertion(const LLTreeNode<T>* node, T* data) = 0;
	virtual void handleRemoval(const LLTreeNode<T>* node, T* data) = 0;
	virtual void handleDestruction(const LLTreeNode<T>* node) = 0;
	virtual void handleStateChange(const LLTreeNode<T>* node) = 0;
};

template <class T>
class LLTreeNode
{
public:
	virtual ~LLTreeNode()
	{
		destroyListeners();
	}

	virtual bool insert(T* data)
	{
		for (U32 i = 0, count = mListeners.size(); i < count; ++i)
		{
			if (mListeners[i].notNull())
			{
				mListeners[i]->handleInsertion(this, data);
			}
		}
		return true;
	}

	virtual bool remove(T* data)
	{
		return true;
	}

	virtual void notifyRemoval(T* data)
	{
		for (U32 i = 0, count = mListeners.size(); i < count; ++i)
		{
			if (mListeners[i].notNull())
			{
				mListeners[i]->handleRemoval(this, data);
			}
		}
	}

	LL_INLINE virtual U32 getListenerCount()
	{
		return mListeners.size();
	}

	LL_INLINE virtual LLTreeListener<T>* getListener(U32 index) const
	{
		if (index < mListeners.size())
		{
			return mListeners[index];
		}
		return NULL;
	}

	LL_INLINE virtual void addListener(LLTreeListener<T>* listener)
	{
		mListeners.push_back(listener);
	}

protected:
	void destroyListeners()
	{
		for (U32 i = 0, count = mListeners.size(); i < count; ++i)
		{
			if (mListeners[i].notNull())
			{
				mListeners[i]->handleDestruction(this);
			}
		}
		mListeners.clear();
	}

public:
	std::vector<LLPointer<LLTreeListener<T> > > mListeners;
};

template <class T>
class LLTreeTraveler
{
public:
	virtual ~LLTreeTraveler() = default;

	virtual void traverse(const LLTreeNode<T>* node) = 0;
	virtual void visit(const LLTreeNode<T>* node) = 0;
};

template <class T>
class LLOctreeListener: public LLTreeListener<T>
{
protected:
	LOG_CLASS(LLOctreeListener<T>);

public:
	typedef LLTreeListener<T> BaseType;
	typedef LLOctreeNode<T> oct_node;

	virtual void handleChildAddition(const oct_node* parent, oct_node* child) = 0;
	virtual void handleChildRemoval(const oct_node* parent, const oct_node* child) = 0;
};

template <class T>
class LLOctreeTraveler
{
protected:
	LOG_CLASS(LLOctreeTraveler<T>);

public:
	virtual void traverse(const LLOctreeNode<T>* node)
	{
		node->accept(this);
		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			traverse(node->getChild(i));
		}
	}

	virtual void visit(const LLOctreeNode<T>* branch) = 0;
};

template <class T>
class LLOctreeTravelerDepthFirst : public LLOctreeTraveler<T>
{
protected:
	LOG_CLASS(LLOctreeTravelerDepthFirst<T>);

public:
	void traverse(const LLOctreeNode<T>* node) override
	{
		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			traverse(node->getChild(i));
		}
		node->accept(this);
	}
};

template <class T>
class LLOctreeNode : public LLTreeNode<T>
{
protected:
	LOG_CLASS(LLOctreeNode<T>);

public:
	typedef LLOctreeTraveler<T>									oct_traveler;
	typedef LLTreeTraveler<T>									tree_traveler;
	typedef std::vector<LLPointer<T> >							element_list;
	typedef LLPointer<T>*										element_iter;
	typedef const LLPointer<T>*									const_element_iter;
	typedef typename std::vector<LLTreeListener<T>*>::iterator	tree_listener_iter;
	typedef LLOctreeNode<T>**									child_list;
	typedef LLOctreeNode<T>**									child_iter;

	typedef LLTreeNode<T>		BaseType;
	typedef LLOctreeNode<T>		oct_node;
	typedef LLOctreeListener<T>	oct_listener;

	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LLOctreeNode(const LLVector4a& center, const LLVector4a& size,
				 BaseType* parent, U8 octant = NO_CHILD_NODES)
	:	mParent((oct_node*)parent),
		mOctant(octant)
	{
		llassert(size[0] >= gOctreeMinSize * 0.5f);
		// Always keep a NULL terminated list to avoid out of bounds exceptions
		// in debug builds
		mData.push_back(NULL);
		mDataEnd = &mData[0];

		mCenter = center;
		mSize = size;

		updateMinMax();
		if (mOctant == NO_CHILD_NODES && mParent)
		{
			mOctant = ((oct_node*)mParent)->getOctant(mCenter);
		}

		mElementCount = 0;

		clearChildren();
	}

	~LLOctreeNode() override
	{
		BaseType::destroyListeners();

		for (U32 i = 0; i < mElementCount; ++i)
		{
			if (mData[i])
			{
				mData[i]->setBinIndex(-1);
				mData[i] = NULL;
			}
			else
			{
				llwarns << "NULL mData[i] found for i = " << i << llendl;
			}
		}
		mData.clear();
		mData.push_back(NULL);
		mDataEnd = &mData[0];

		for (U32 i = 0; i < getChildCount(); ++i)
		{
			delete getChild(i);
		}
	}

	LL_INLINE const BaseType* getParent()	const			{ return mParent; }
	LL_INLINE void setParent(BaseType* parent)				{ mParent = (oct_node*)parent; }
	LL_INLINE const LLVector4a& getCenter() const			{ return mCenter; }
	LL_INLINE const LLVector4a& getSize() const				{ return mSize; }
	LL_INLINE void setCenter(const LLVector4a& center)		{ mCenter = center; }
	LL_INLINE void setSize(const LLVector4a& size)			{ mSize = size; }
	LL_INLINE oct_node* getNodeAt(T* data)					{ return getNodeAt(data->getPositionGroup(), data->getBinRadius()); }
	LL_INLINE U8 getOctant() const							{ return mOctant; }
	LL_INLINE const oct_node* getOctParent() const			{ return (const oct_node*)getParent(); }
	LL_INLINE oct_node* getOctParent() 						{ return (oct_node*)getParent(); }

	U8 getOctant(const LLVector4a& pos) const	// get the octant pos is in
	{
		return (U8)(pos.greaterThan(mCenter).getGatheredBits() & 0x7);
	}

	LL_INLINE bool isInside(const LLVector4a& pos, F32 rad) const
	{
		return rad <= mSize[0] * 2.f && isInside(pos);
	}

	LL_INLINE bool isInside(T* data) const
	{
		return isInside(data->getPositionGroup(), data->getBinRadius());
	}

	bool isInside(const LLVector4a& pos) const
	{
		if (pos.greaterThan(mMax).getGatheredBits() & 0x7)
		{
			return false;
		}
		return !(pos.lessEqual(mMin).getGatheredBits() & 0x7);
	}

	void updateMinMax()
	{
		mMax.setAdd(mCenter, mSize);
		mMin.setSub(mCenter, mSize);
	}

	LL_INLINE oct_listener* getOctListener(U32 index)
	{
		return (oct_listener*)BaseType::getListener(index);
	}

	LL_INLINE bool contains(T* xform)
	{
		return contains(xform->getBinRadius());
	}

	bool contains(F32 radius)
	{
		if (!mParent)
		{
			// Root node contains nothing
			return false;
		}

		F32 size = mSize[0];
		F32 p_size = size * 2.f;

		return (radius <= gOctreeMinSize && size <= gOctreeMinSize) ||
			   (radius <= p_size && radius > size);
	}

	static void pushCenter(LLVector4a& center, const LLVector4a& size,
						   const T* data)
	{
		const LLVector4a& pos = data->getPositionGroup();

		LLVector4Logical gt = pos.greaterThan(center);

		LLVector4a up;
		up = _mm_and_ps(size, gt);

		LLVector4a down;
		down = _mm_andnot_ps(gt, size);

		center.add(up);
		center.sub(down);
	}

	void accept(oct_traveler* visitor)				{ visitor->visit(this); }
	virtual bool isLeaf() const						{ return mChildCount == 0; }

	U32 getElementCount() const						{ return mElementCount; }
	bool isEmpty() const							{ return mElementCount == 0; }
	element_list& getData()							{ return mData; }
	const element_list& getData() const				{ return mData; }
	element_iter getDataBegin()						{ return &mData[0]; }
	element_iter getDataEnd()						{ return mDataEnd; }
	const_element_iter getDataBegin() const			{ return &mData[0]; }
	const_element_iter getDataEnd() const			{ return mDataEnd; }

	U32 getChildCount()	const						{ return mChildCount; }
	oct_node* getChild(U32 index)					{ return mChild[index]; }
	const oct_node* getChild(U32 index) const		{ return mChild[index]; }
	child_list& getChildren()						{ return mChild; }
	const child_list& getChildren() const			{ return mChild; }

	void accept(tree_traveler* visitor) const		{ visitor->visit(this); }
	void accept(oct_traveler* visitor) const		{ visitor->visit(this); }

	void validateChildMap()
	{
		for (U32 i = 0; i < 8; ++i)
		{
			U8 idx = mChildMap[i];
			if (idx != NO_CHILD_NODES)
			{
				LLOctreeNode<T>* child = mChild[idx];
				if (!child)
				{
					llerrs << "NULL child !" << llendl;
				}
				if (child->getOctant() != i)
				{
					llerrs << "Invalid child map, bad octant data." << llendl;
				}
				if (getOctant(child->getCenter()) != child->getOctant())
				{
					llerrs << "Invalid child octant compared to position data."
						   << llendl;
				}
			}
		}
	}

	oct_node* getNodeAt(const LLVector4a& pos, F32 rad)
	{
		LLOctreeNode<T>* node = this;

		if (node->isInside(pos, rad))
		{
			// Do a quick search by octant
			U8 octant = node->getOctant(pos);

			// Traverse the tree until we find a node that has no node at the
			// appropriate octant or is smaller than the object. By definition,
			// that node is the smallest node that contains the data
			U8 next_node = node->mChildMap[octant];

			while (next_node != NO_CHILD_NODES && node->getSize()[0] >= rad)
			{
				node = node->getChild(next_node);
				octant = node->getOctant(pos);
				next_node = node->mChildMap[octant];
			}
		}
		else if (!node->contains(rad) && node->getParent())
		{
			// If we got here, data does not exist in this node
			return ((LLOctreeNode<T>*)node->getParent())->getNodeAt(pos, rad);
		}

		return node;
	}

	bool insert(T* data) override
	{
		if (!data || data->getBinIndex() != -1)
		{
			llwarns << "Invalid element added to octree branch !" << llendl;
			return false;
		}
		LLOctreeNode<T>* parent = getOctParent();

		// Is it here ?
		const LLVector4a& pos_group = data->getPositionGroup();
		if (isInside(pos_group))
		{
			if (((getElementCount() < gOctreeMaxCapacity ||
				  getSize()[0] <= gOctreeMinSize) &&
				 contains(data->getBinRadius())) ||
				(data->getBinRadius() > getSize()[0] &&	parent &&
				 parent->getElementCount() >= gOctreeMaxCapacity))
			{
				// It belongs here
				mData.push_back(NULL);
				mData[mElementCount] = data;
				mDataEnd = &mData[++mElementCount];
				data->setBinIndex(mElementCount - 1);
				return BaseType::insert(data);
			}

			// Find a child to give it to
			oct_node* child = NULL;
			for (U32 i = 0; i < getChildCount(); ++i)
			{
				child = getChild(i);
				if (child->isInside(pos_group))
				{
					return child->insert(data);
				}
			}

			// It is here, but no kids are in the right place, make a new kid
			LLVector4a center = getCenter();
			LLVector4a size = getSize();
			size.mul(0.5f);

			// Push center in direction of data
			LLOctreeNode<T>::pushCenter(center, size, data);

			// Handle case where floating point number gets too small
			LLVector4a val;
			val.setSub(center, getCenter());
			val.setAbs(val);

			LLVector4a min_diff(gOctreeMinSize);
			if ((val.lessThan(min_diff).getGatheredBits() & 0x7) == 0x7)
			{
				mData.push_back(NULL);
				mData[mElementCount] = data;
				mDataEnd = &mData[++mElementCount];
				data->setBinIndex(mElementCount - 1);
				return BaseType::insert(data);
			}

#if LL_OCTREE_PARANOIA_CHECK
			if (getChildCount() == 8)
			{
				// This really is not possible, something bad has happened
				llwarns << "Octree detected floating point error and gave up."
						<< llendl;
				return false;
			}

			// Make sure no existing node matches this position
			for (U32 i = 0; i < getChildCount(); ++i)
			{
				if (mChild[i]->getCenter().equals3(center))
				{
					llwarns << "Octree detected duplicate child center and gave up."
							<< llendl;
					return false;
				}
			}
#endif
			llassert(size[0] >= gOctreeMinSize * 0.5f);
			// Make the new child
			child = new LLOctreeNode<T>(center, size, this);
			addChild(child);
			return child->insert(data);
		}
#if 1	// We used to crash in here, but we did not yet test for parent...
		else if (parent)
		{
			// It is not in here, give it to the root
			llwarns << "Octree insertion failed, starting over from root !"
					<< llendl;

			oct_node* node = this;

			while (parent)
			{
				node = parent;
				parent = node->getOctParent();
			}

			return node->insert(data);
		}
#endif
		else
		{
			llwarns << "Octree insertion failed !" << llendl;
			return false;
		}
	}

	void _remove(T* data, S32 i)
	{
		// Precondition -- mElementCount > 0, i is in range [0, mElementCount)
		if (mElementCount == 0 || i < 0 || i > (S32)mElementCount)
		{
			llwarns << "Index out of range: mElementCount = " << mElementCount
					<< " - index = " << i << " - Aborted." << llendl;
			return;
		}

		--mElementCount;
		data->setBinIndex(-1);

		if (mElementCount > 0)
		{
			if ((S32)mElementCount != i)
			{
				// Might unref data, do not access data after this point
				mData[i] = mData[mElementCount];
				mData[i]->setBinIndex(i);
			}

			mData[mElementCount] = NULL;
			mData.pop_back();
			mDataEnd = &mData[mElementCount];
		}
		else
		{
			mData.clear();
			mData.push_back(NULL);
			mDataEnd = &mData[0];
		}

		this->notifyRemoval(data);
		checkAlive();
	}

	bool remove(T* data) override
	{
		S32 i = data->getBinIndex();
		if (i >= 0 && i < (S32)mElementCount)
		{
			if (mData[i] == data)
			{
				// found it
				_remove(data, i);
				llassert(data->getBinIndex() == -1);
				return true;
			}
		}

		if (isInside(data))
		{
			oct_node* dest = getNodeAt(data);
			if (dest != this)
			{
				bool ret = dest->remove(data);
				llassert(data->getBinIndex() == -1);
				return ret;
			}
		}

		// SHE IS GONE MISSING...
		// None of the children have it, let's just brute force this bastard
		// out starting with the root node (UGLY CODE COMETH !)
		oct_node* parent = getOctParent();
		oct_node* node = this;
		while (parent)
		{
			node = parent;
			parent = node->getOctParent();
		}

		// Node is now root
		llwarns << "Octree removing element by address, severe performance penalty !"
				<< llendl;
		node->removeByAddress(data);
		llassert(data->getBinIndex() == -1);
		return true;
	}

	void removeByAddress(T* data)
	{
        for (U32 i = 0; i < mElementCount; ++i)
		{
			if (mData[i] == data)
			{
				// we have data
				_remove(data, i);
				return;
			}
		}

		for (U32 i = 0; i < getChildCount(); ++i)
		{
			// We do not contain data, so pass this guy down
			LLOctreeNode<T>* child = (LLOctreeNode<T>*)getChild(i);
			child->removeByAddress(data);
		}
	}

	void clearChildren()
	{
		mChildCount = 0;
		memset((void*)mChildMap, NO_CHILD_NODES, sizeof(mChildMap));
	}

	void validate()
	{
#if LL_OCTREE_PARANOIA_CHECK
		for (U32 i = 0; i < getChildCount(); ++i)
		{
			mChild[i]->validate();
			if (mChild[i]->getParent() != this)
			{
				llerrs << "Octree child has invalid parent." << llendl;
			}
		}
#endif
	}

	virtual bool balance()
	{
		return false;
	}

	void destroy()
	{
		for (U32 i = 0; i < getChildCount(); ++i)
		{
			mChild[i]->destroy();
			delete mChild[i];
		}
	}

	void addChild(oct_node* child, bool silent = false)
	{
#if LL_OCTREE_PARANOIA_CHECK

		if (child->getSize().equals3(getSize()))
		{
			llwarns << "Child size is same as parent size !" << llendl;
		}

		for (U32 i = 0; i < getChildCount(); ++i)
		{
			if (!mChild[i]->getSize().equals3(child->getSize()))
			{
				llwarns <<"Invalid octree child size." << llendl;
			}
			if (mChild[i]->getCenter().equals3(child->getCenter()))
			{
				llwarns <<"Duplicate octree child position." << llendl;
			}
		}

		if (mChild.size() >= 8)
		{
			llwarns <<"Octree node has too many children... Why ?" << llendl;
		}
#endif

		mChildMap[child->getOctant()] = mChildCount;

		mChild[mChildCount++] = child;
		child->setParent(this);

		if (!silent)
		{
			for (U32 i = 0; i < this->getListenerCount(); ++i)
			{
				oct_listener* listener = this->getOctListener(i);
				if (listener)
				{
					listener->handleChildAddition(this, child);
				}
				else
				{
					llwarns << "NULL listener found !" << llendl;
				}
			}
		}
	}

	void removeChild(S32 index, bool destroy = false)
	{
		for (U32 i = 0; i < this->getListenerCount(); ++i)
		{
			oct_listener* listener = this->getOctListener(i);
			if (listener)
			{
				listener->handleChildRemoval(this, getChild(index));
			}
			else
			{
				llwarns << "NULL listener found !" << llendl;
			}
		}

		if (destroy)
		{
			mChild[index]->destroy();
			delete mChild[index];
		}

		mChild[index] = mChild[--mChildCount];

		// Rebuild child map
		memset((void*)mChildMap, NO_CHILD_NODES, sizeof(mChildMap));

		for (U32 i = 0; i < mChildCount; ++i)
		{
			mChildMap[mChild[i]->getOctant()] = i;
		}

		checkAlive();
	}

	void checkAlive()
	{
		if (getChildCount() == 0 && getElementCount() == 0)
		{
			oct_node* parent = getOctParent();
			if (parent)
			{
				parent->deleteChild(this);
			}
		}
	}

	void deleteChild(oct_node* node)
	{
		for (U32 i = 0; i < getChildCount(); ++i)
		{
			if (getChild(i) == node)
			{
				removeChild(i, true);
				return;
			}
		}

		llwarns << "Octree failed to delete requested child." << llendl;
	}

protected:
	typedef enum
	{
		CENTER = 0,
		SIZE = 1,
		MAX = 2,
		MIN = 3
	} eDName;

	LLVector4a			mCenter;
	LLVector4a			mSize;
	LLVector4a			mMax;
	LLVector4a			mMin;

	LLOctreeNode<T>*	mChild[8];
	U8					mChildMap[8];
	U32					mChildCount;

	U32					mElementCount;
	element_list		mData;
	element_iter		mDataEnd;

	oct_node*			mParent;
	U8					mOctant;
};

// Just like a regular node, except it might expand on insert and compress on
// balance.
template <class T>
class LLOctreeRoot : public LLOctreeNode<T>
{
protected:
	LOG_CLASS(LLOctreeRoot<T>);

public:
	typedef LLOctreeNode<T>	BaseType;
	typedef LLOctreeNode<T> oct_node;

	LLOctreeRoot(const LLVector4a& center, const LLVector4a& size,
				 BaseType* parent)
	:	BaseType(center, size, parent)
	{
	}

	bool balance()
	{
		if (this->getChildCount() == 1 && !this->mChild[0]->isLeaf() &&
			this->mChild[0]->getElementCount() == 0)
		{
			// If we have only one child and that child is an empty branch,
			// make that child the root
			oct_node* child = this->mChild[0];

			// Make the root node look like the child
			this->setCenter(this->mChild[0]->getCenter());
			this->setSize(this->mChild[0]->getSize());
			this->updateMinMax();

			// Reset root node child list
			this->clearChildren();

			// Copy the child's children into the root node silently (do not
			// notify listeners of addition)
			for (U32 i = 0; i < child->getChildCount(); ++i)
			{
				this->addChild(child->getChild(i), true);
			}

			// Destroy child
			child->clearChildren();
			delete child;

			return false;
		}

		return true;
	}

	// LLOctreeRoot::insert
	bool insert(T* data)
	{
		if (!data)
		{
			llwarns_once << "Attempt to add a NULL element added to octree root !"
						 << llendl;
			return false;
		}

		S32 binradius = data->getBinRadius();
		if (binradius > 4096.f)
		{
			llwarns_once << "Element exceeds maximum size in octree root !"
						 << llendl;
			return false;
		}


		const LLVector4a& pos_group = data->getPositionGroup();

		LLVector4a val;
		val.setSub(pos_group, BaseType::mCenter);
		val.setAbs(val);
		if ((val.lessThan(gOctreeMaxMag).getGatheredBits() & 0x7) != 0x7)
		{
			llwarns_once << "Element exceeds range of spatial partition !  Insertion skipped, expect occlusion issues."
						 << llendl;
			return false;
		}

		if (this->getSize()[0] > binradius && this->isInside(pos_group))
		{
			// We got it, just act like a branch
			oct_node* node = this->getNodeAt(data);
			if (node == this)
			{
				return LLOctreeNode<T>::insert(data);
			}
			if (node->isInside(pos_group))
			{
				return node->insert(data);
			}

			llwarns << "Failed to insert data at child node" << llendl;
			return false;
		}

		if (this->getChildCount() == 0)
		{
			// First object being added, just wrap it up
			LLVector4a center, size;
			while (!(this->getSize()[0] > binradius &&
				   this->isInside(pos_group)))
			{
				center = this->getCenter();
				size = this->getSize();
				LLOctreeNode<T>::pushCenter(center, size, data);
				this->setCenter(center);
				size.mul(2.f);
				this->setSize(size);
				this->updateMinMax();
			}
			return LLOctreeNode<T>::insert(data);
		}

		while (!(this->getSize()[0] > binradius && this->isInside(pos_group)))
		{
			// The data is outside the root node, we need to grow
			LLVector4a center(this->getCenter());
			LLVector4a size(this->getSize());

			// Expand this node
			LLVector4a newcenter(center);
			LLOctreeNode<T>::pushCenter(newcenter, size, data);
			this->setCenter(newcenter);
			LLVector4a size2 = size;
			size2.mul(2.f);
			this->setSize(size2);
			this->updateMinMax();

			llassert(size[0] >= gOctreeMinSize);

			// Copy our children to a new branch
			LLOctreeNode<T>* newnode = new LLOctreeNode<T>(center, size, this);

			for (U32 i = 0; i < this->getChildCount(); ++i)
			{
				LLOctreeNode<T>* child = this->getChild(i);
				newnode->addChild(child);
			}

			// Clear our children and add the root copy
			this->clearChildren();
			this->addChild(newnode);
		}

		// Insert the data
		return insert(data);
	}
};

#endif
