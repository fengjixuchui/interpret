// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <ebm@koch.ninja>

#ifndef EBM_BOOSTING_STATE_H
#define EBM_BOOSTING_STATE_H

#include <stdlib.h> // malloc, realloc, free
#include <stddef.h> // size_t, ptrdiff_t
#include <limits> // numeric_limits

#include "ebm_native.h"
#include "EbmInternal.h"
// very independent includes
#include "Logging.h" // EBM_ASSERT & LOG
#include "RandomStream.h"
#include "SegmentedTensor.h"
// this depends on TreeNode pointers, but doesn't require the full definition of TreeNode
#include "CachedThreadResources.h"
// feature includes
#include "Feature.h"
// FeatureCombination.h depends on FeatureInternal.h
#include "FeatureCombination.h"
// dataset depends on features
#include "DataSetByFeatureCombination.h"
// samples is somewhat independent from datasets, but relies on an indirect coupling with them
#include "SamplingWithReplacement.h"

union CachedThreadResourcesUnion {
   CachedBoostingThreadResources<false> regression;
   CachedBoostingThreadResources<true> classification;

   EBM_INLINE CachedThreadResourcesUnion(const ptrdiff_t runtimeLearningTypeOrCountTargetClasses) {
      LOG_N(TraceLevelInfo, "Entered CachedThreadResourcesUnion: runtimeLearningTypeOrCountTargetClasses=%td", runtimeLearningTypeOrCountTargetClasses);
      const size_t cVectorLength = GetVectorLength(runtimeLearningTypeOrCountTargetClasses);
      if(IsClassification(runtimeLearningTypeOrCountTargetClasses)) {
         // member classes inside a union requre explicit call to constructor
         new(&classification) CachedBoostingThreadResources<true>(cVectorLength);
      } else {
         EBM_ASSERT(IsRegression(runtimeLearningTypeOrCountTargetClasses));
         // member classes inside a union requre explicit call to constructor
         new(&regression) CachedBoostingThreadResources<false>(cVectorLength);
      }
      LOG_0(TraceLevelInfo, "Exited CachedThreadResourcesUnion");
   }

   EBM_INLINE ~CachedThreadResourcesUnion() {
      // TODO: figure out why this is being called, and if that is bad!
      //LOG_0(TraceLevelError, "ERROR ~CachedThreadResourcesUnion called.  It's union destructors should be called explicitly");

      // we don't have enough information here to delete this object, so we do it from our caller
      // we still need this destructor for a technicality that it might be called
      // if there were an excpetion generated in the initializer list which it is constructed in
      // but we have been careful to ensure that the class we are including it in doesn't thow exceptions in the
      // initializer list
   }
};

class EbmBoostingState {
public:
   const ptrdiff_t m_runtimeLearningTypeOrCountTargetClasses;

   const size_t m_cFeatureCombinations;
   FeatureCombination ** const m_apFeatureCombinations;

   // TODO : can we internalize these so that they are not pointers and are therefore subsumed into our class
   DataSetByFeatureCombination * m_pTrainingSet;
   DataSetByFeatureCombination * m_pValidationSet;

   const size_t m_cSamplingSets;

   SamplingMethod ** m_apSamplingSets;
   SegmentedTensor ** m_apCurrentModel;
   SegmentedTensor ** m_apBestModel;

   FloatEbmType m_bestModelMetric;

   SegmentedTensor * const m_pSmallChangeToModelOverwriteSingleSamplingSet;
   SegmentedTensor * const m_pSmallChangeToModelAccumulatedFromSamplingSets;

   const size_t m_cFeatures;
   // TODO : in the future, we can allocate this inside a function so that even the objects inside are const
   Feature * const m_aFeatures;

   RandomStream m_randomStream;

   // TODO CachedThreadResourcesUnion has a lot of things that aren't per thread.  Right now it's functioning as a place to put mostly things
   // that are different between regression and classification.  In the future we'll want something like it for breaking the work into workable chunks
   // so I'm leaving it here for now.  
   // m_pSmallChangeToModelOverwriteSingleSamplingSet, m_pSmallChangeToModelAccumulatedFromSamplingSets and m_aEquivalentSplits should eventually move into 
   // the per-chunk class and we'll need a per-chunk m_randomStream that is initialized with it's own predictable seed 
   CachedThreadResourcesUnion m_cachedThreadResourcesUnion;

   EBM_INLINE EbmBoostingState(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses, 
      const size_t cFeatures, 
      const size_t cFeatureCombinations, 
      const size_t cSamplingSets, 
      const IntEbmType randomSeed
   )
      : m_runtimeLearningTypeOrCountTargetClasses(runtimeLearningTypeOrCountTargetClasses)
      , m_cFeatureCombinations(cFeatureCombinations)
      , m_apFeatureCombinations(0 == cFeatureCombinations ? nullptr : FeatureCombination::AllocateFeatureCombinations(cFeatureCombinations))
      , m_pTrainingSet(nullptr)
      , m_pValidationSet(nullptr)
      , m_cSamplingSets(cSamplingSets)
      , m_apSamplingSets(nullptr)
      , m_apCurrentModel(nullptr)
      , m_apBestModel(nullptr)
      , m_bestModelMetric(FloatEbmType { std::numeric_limits<FloatEbmType>::max() })
      , m_pSmallChangeToModelOverwriteSingleSamplingSet(
         SegmentedTensor::Allocate(k_cDimensionsMax, GetVectorLength(runtimeLearningTypeOrCountTargetClasses)))
      , m_pSmallChangeToModelAccumulatedFromSamplingSets(
         SegmentedTensor::Allocate(k_cDimensionsMax, GetVectorLength(runtimeLearningTypeOrCountTargetClasses)))
      , m_cFeatures(cFeatures)
      , m_aFeatures(0 == cFeatures || IsMultiplyError(sizeof(Feature), cFeatures) ? nullptr : static_cast<Feature *>(malloc(sizeof(Feature) * cFeatures)))
      , m_randomStream(randomSeed)
      // we catch any errors in the constructor, so this should not be able to throw
      , m_cachedThreadResourcesUnion(runtimeLearningTypeOrCountTargetClasses) {
   }

   EBM_INLINE ~EbmBoostingState() {
      LOG_0(TraceLevelInfo, "Entered ~EbmBoostingState");

      if(IsClassification(m_runtimeLearningTypeOrCountTargetClasses)) {
         // member classes inside a union requre explicit call to destructor
         LOG_0(TraceLevelInfo, "~EbmBoostingState identified as classification type");
         m_cachedThreadResourcesUnion.classification.~CachedBoostingThreadResources();
      } else {
         EBM_ASSERT(IsRegression(m_runtimeLearningTypeOrCountTargetClasses));
         // member classes inside a union requre explicit call to destructor
         LOG_0(TraceLevelInfo, "~EbmBoostingState identified as regression type");
         m_cachedThreadResourcesUnion.regression.~CachedBoostingThreadResources();
      }

      SamplingWithReplacement::FreeSamplingSets(m_cSamplingSets, m_apSamplingSets);

      delete m_pTrainingSet;
      delete m_pValidationSet;

      FeatureCombination::FreeFeatureCombinations(m_cFeatureCombinations, m_apFeatureCombinations);

      free(m_aFeatures);

      DeleteSegmentedTensors(m_cFeatureCombinations, m_apCurrentModel);
      DeleteSegmentedTensors(m_cFeatureCombinations, m_apBestModel);
      SegmentedTensor::Free(m_pSmallChangeToModelOverwriteSingleSamplingSet);
      SegmentedTensor::Free(m_pSmallChangeToModelAccumulatedFromSamplingSets);

      LOG_0(TraceLevelInfo, "Exited ~EbmBoostingState");
   }

   static void DeleteSegmentedTensors(const size_t cFeatureCombinations, SegmentedTensor ** const apSegmentedTensors);
   static SegmentedTensor ** InitializeSegmentedTensors(
      const size_t cFeatureCombinations, 
      const FeatureCombination * const * const apFeatureCombinations, 
      const size_t cVectorLength
   );
   bool Initialize(
      const EbmNativeFeature * const aFeatures, 
      const EbmNativeFeatureCombination * const aFeatureCombinations, 
      const IntEbmType * featureCombinationIndexes, 
      const size_t cTrainingInstances, 
      const void * const aTrainingTargets, 
      const IntEbmType * const aTrainingBinnedData, 
      const FloatEbmType * const aTrainingPredictorScores, 
      const size_t cValidationInstances, 
      const void * const aValidationTargets, 
      const IntEbmType * const aValidationBinnedData, 
      const FloatEbmType * const aValidationPredictorScores
   );
};

#endif // EBM_BOOSTING_STATE_H
