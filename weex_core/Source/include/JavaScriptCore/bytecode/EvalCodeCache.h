/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#pragma once

#include "EvalExecutable.h"
#include "JSGlobalObject.h"
#include "JSScope.h"
#include "Options.h"
#include "SourceCode.h"
#include "SourceCodeKey.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace JSC {

    class SlotVisitor;

    class EvalCodeCache {
    public:
        // Specialized cache key (compared with SourceCodeKey) for eval code cache.
        class CacheKey {
        public:
            CacheKey(const String& source, CallSiteIndex callSiteIndex)
                : m_source(source.impl())
                , m_callSiteIndex(callSiteIndex)
            {
            }

            CacheKey(WTF::HashTableDeletedValueType)
                : m_source(WTF::HashTableDeletedValue)
            {
            }

            CacheKey() = default;

            unsigned hash() const { return m_source->hash() ^ m_callSiteIndex.bits(); }

            bool isEmptyValue() const { return !m_source; }

            bool operator==(const CacheKey& other) const
            {
                return m_callSiteIndex == other.m_callSiteIndex && WTF::equal(m_source.get(), other.m_source.get());
            }

            bool isHashTableDeletedValue() const { return m_source.isHashTableDeletedValue(); }

            struct Hash {
                static unsigned hash(const CacheKey& key)
                {
                    return key.hash();
                }
                static bool equal(const CacheKey& lhs, const CacheKey& rhs)
                {
                    return lhs == rhs;
                }
                static const bool safeToCompareToEmptyOrDeleted = false;
            };

            typedef SimpleClassHashTraits<CacheKey> HashTraits;

        private:
            RefPtr<StringImpl> m_source;
            CallSiteIndex m_callSiteIndex;
        };

        EvalExecutable* tryGet(const String& evalSource, CallSiteIndex callSiteIndex)
        {
            return m_cacheMap.fastGet(CacheKey(evalSource, callSiteIndex)).get();
        }
        
        EvalExecutable* getSlow(ExecState* exec, JSCell* owner, const String& evalSource, CallSiteIndex callSiteIndex, bool inStrictContext, DerivedContextType derivedContextType, EvalContextType evalContextType, bool isArrowFunctionContext, JSScope* scope)
        {
            VariableEnvironment variablesUnderTDZ;
            JSScope::collectVariablesUnderTDZ(scope, variablesUnderTDZ);
            EvalExecutable* evalExecutable = EvalExecutable::create(exec, makeSource(evalSource), inStrictContext, derivedContextType, isArrowFunctionContext, evalContextType, &variablesUnderTDZ);
            if (!evalExecutable)
                return nullptr;

            if (m_cacheMap.size() < maxCacheEntries)
                m_cacheMap.set(CacheKey(evalSource, callSiteIndex), WriteBarrier<EvalExecutable>(exec->vm(), owner, evalExecutable));

            return evalExecutable;
        }

        bool isEmpty() const { return m_cacheMap.isEmpty(); }

        void visitAggregate(SlotVisitor&);

        void clear()
        {
            m_cacheMap.clear();
        }

    private:
        static const int maxCacheEntries = 64;

        typedef HashMap<CacheKey, WriteBarrier<EvalExecutable>, CacheKey::Hash, CacheKey::HashTraits> EvalCacheMap;
        EvalCacheMap m_cacheMap;
    };

} // namespace JSC
