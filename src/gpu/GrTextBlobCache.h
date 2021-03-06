/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTextBlobCache_DEFINED
#define GrTextBlobCache_DEFINED

#include "GrAtlasTextContext.h"
#include "SkTDynamicHash.h"
#include "SkTextBlob.h"

class GrTextBlobCache {
public:
    typedef GrAtlasTextContext::BitmapTextBlob BitmapTextBlob;

    GrTextBlobCache() : fPool(kPreAllocSize, kMinGrowthSize) {}
    ~GrTextBlobCache();

    // creates an uncached blob
    BitmapTextBlob* createBlob(int glyphCount, int runCount, size_t maxVASize);

    BitmapTextBlob* createCachedBlob(const SkTextBlob* blob, size_t maxVAStride) {
        int glyphCount = 0;
        int runCount = 0;
        BlobGlyphCount(&glyphCount, &runCount, blob);
        BitmapTextBlob* cacheBlob = this->createBlob(glyphCount, runCount, maxVAStride);
        cacheBlob->fUniqueID = blob->uniqueID();
        this->add(cacheBlob);
        return cacheBlob;
    }

    BitmapTextBlob* find(uint32_t uniqueID) {
        return fCache.find(uniqueID);
    }

    void remove(BitmapTextBlob* blob) {
        fCache.remove(blob->fUniqueID);
        fBlobList.remove(blob);
        blob->unref();
    }

    void add(BitmapTextBlob* blob) {
        fCache.add(blob);
        fBlobList.addToHead(blob);

        // If we are overbudget, then unref until we are below budget again
        if (fPool.size() > kBudget) {
            BitmapBlobList::Iter iter;
            iter.init(fBlobList, BitmapBlobList::Iter::kTail_IterStart);
            BitmapTextBlob* lruBlob = iter.get();
            SkASSERT(lruBlob);
            do {
                fCache.remove(lruBlob->fUniqueID);
                fBlobList.remove(lruBlob);
                lruBlob->unref();
                iter.prev();
            } while (fPool.size() > kBudget && (lruBlob = iter.get()));
        }
    }

    void makeMRU(BitmapTextBlob* blob) {
        if (fBlobList.head() == blob) {
            return;
        }

        fBlobList.remove(blob);
        fBlobList.addToHead(blob);
    }

private:
    // TODO move to SkTextBlob
    void BlobGlyphCount(int* glyphCount, int* runCount, const SkTextBlob* blob) {
        SkTextBlob::RunIterator itCounter(blob);
        for (; !itCounter.done(); itCounter.next(), (*runCount)++) {
            *glyphCount += itCounter.glyphCount();
        }
    }

    typedef SkTInternalLList<BitmapTextBlob> BitmapBlobList;

    // Budget was chosen to be ~4 megabytes.  The min alloc and pre alloc sizes in the pool are
    // based off of the largest cached textblob I have seen in the skps(a couple of kilobytes).
    static const int kPreAllocSize = 1 << 17;
    static const int kMinGrowthSize = 1 << 17;
    static const int kBudget = 1 << 20;
    BitmapBlobList fBlobList;
    SkTDynamicHash<BitmapTextBlob, uint32_t> fCache;
    GrMemoryPool fPool;
};

#endif
