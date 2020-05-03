// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#include "PrecompiledHeader.h"

#include <string.h> // memset
#include <stdlib.h> // malloc, realloc, free
#include <stddef.h> // size_t, ptrdiff_t
#include <limits> // numeric_limits

#include "ebm_native.h"
#include "EbmInternal.h"
#include "Logging.h" // EBM_ASSERT & LOG
// feature includes
#include "Feature.h"
// dataset depends on features
#include "DataSetByFeature.h"
// depends on the above
#include "DimensionMultiple.h"

#include "EbmInteractionState.h"

// a*PredictorScores = logOdds for binary classification
// a*PredictorScores = logWeights for multiclass classification
// a*PredictorScores = predictedValue for regression
EbmInteractionState * AllocateInteraction(
   IntEbmType countFeatures, 
   const EbmNativeFeature * features, 
   const ptrdiff_t runtimeLearningTypeOrCountTargetClasses, 
   IntEbmType countInstances, 
   const void * targets, 
   const IntEbmType * binnedData, 
   const FloatEbmType * predictorScores,
   const FloatEbmType * const optionalTempParams
) {
   // TODO : give AllocateInteraction the same calling parameter order as InitializeInteractionClassification

   EBM_ASSERT(0 <= countFeatures);
   EBM_ASSERT(0 == countFeatures || nullptr != features);
   // countTargetClasses is checked by our caller since it's only valid for classification at this point
   EBM_ASSERT(0 <= countInstances);
   EBM_ASSERT(0 == countInstances || nullptr != targets);
   EBM_ASSERT(0 == countInstances || 0 == countFeatures || nullptr != binnedData);
   EBM_ASSERT(0 == countInstances || nullptr != predictorScores);

   if(!IsNumberConvertable<size_t, IntEbmType>(countFeatures)) {
      LOG_0(TraceLevelWarning, "WARNING AllocateInteraction !IsNumberConvertable<size_t, IntEbmType>(countFeatures)");
      return nullptr;
   }
   if(!IsNumberConvertable<size_t, IntEbmType>(countInstances)) {
      LOG_0(TraceLevelWarning, "WARNING AllocateInteraction !IsNumberConvertable<size_t, IntEbmType>(countInstances)");
      return nullptr;
   }

   size_t cFeatures = static_cast<size_t>(countFeatures);
   size_t cInstances = static_cast<size_t>(countInstances);

   LOG_0(TraceLevelInfo, "Entered EbmInteractionState");
   EbmInteractionState * const pEbmInteractionState = new (std::nothrow) EbmInteractionState(
      runtimeLearningTypeOrCountTargetClasses, 
      cFeatures, 
      optionalTempParams
   );
   LOG_N(TraceLevelInfo, "Exited EbmInteractionState %p", static_cast<void *>(pEbmInteractionState));
   if(UNLIKELY(nullptr == pEbmInteractionState)) {
      LOG_0(TraceLevelWarning, "WARNING AllocateInteraction nullptr == pEbmInteractionState");
      return nullptr;
   }
   if(UNLIKELY(pEbmInteractionState->InitializeInteraction(features, cInstances, targets, binnedData, predictorScores))) {
      LOG_0(TraceLevelWarning, "WARNING AllocateInteraction pEbmInteractionState->InitializeInteraction");
      delete pEbmInteractionState;
      return nullptr;
   }
   return pEbmInteractionState;
}

EBM_NATIVE_IMPORT_EXPORT_BODY PEbmInteraction EBM_NATIVE_CALLING_CONVENTION InitializeInteractionClassification(
   IntEbmType countTargetClasses,
   IntEbmType countFeatures,
   const EbmNativeFeature * features,
   IntEbmType countInstances,
   const IntEbmType * binnedData,
   const IntEbmType * targets,
   const FloatEbmType * predictorScores,
   const FloatEbmType * optionalTempParams
) {
   LOG_N(
      TraceLevelInfo, 
      "Entered InitializeInteractionClassification: countTargetClasses=%" IntEbmTypePrintf ", countFeatures=%" IntEbmTypePrintf 
      ", features=%p, countInstances=%" IntEbmTypePrintf ", binnedData=%p, targets=%p, predictorScores=%p, optionalTempParams=%p",
      countTargetClasses, 
      countFeatures, 
      static_cast<const void *>(features), 
      countInstances, 
      static_cast<const void *>(binnedData), 
      static_cast<const void *>(targets), 
      static_cast<const void *>(predictorScores),
      static_cast<const void *>(optionalTempParams)
   );
   if(countTargetClasses < 0) {
      LOG_0(TraceLevelError, "ERROR InitializeInteractionClassification countTargetClasses can't be negative");
      return nullptr;
   }
   if(0 == countTargetClasses && 0 != countInstances) {
      LOG_0(TraceLevelError, "ERROR InitializeInteractionClassification countTargetClasses can't be zero unless there are no instances");
      return nullptr;
   }
   if(!IsNumberConvertable<ptrdiff_t, IntEbmType>(countTargetClasses)) {
      LOG_0(TraceLevelWarning, "WARNING InitializeInteractionClassification !IsNumberConvertable<ptrdiff_t, IntEbmType>(countTargetClasses)");
      return nullptr;
   }
   const ptrdiff_t runtimeLearningTypeOrCountTargetClasses = static_cast<ptrdiff_t>(countTargetClasses);
   PEbmInteraction pEbmInteraction = reinterpret_cast<PEbmInteraction>(AllocateInteraction(
      countFeatures, 
      features, 
      runtimeLearningTypeOrCountTargetClasses, 
      countInstances, 
      targets, 
      binnedData, 
      predictorScores,
      optionalTempParams
   ));
   LOG_N(TraceLevelInfo, "Exited InitializeInteractionClassification %p", static_cast<void *>(pEbmInteraction));
   return pEbmInteraction;
}

EBM_NATIVE_IMPORT_EXPORT_BODY PEbmInteraction EBM_NATIVE_CALLING_CONVENTION InitializeInteractionRegression(
   IntEbmType countFeatures,
   const EbmNativeFeature * features,
   IntEbmType countInstances,
   const IntEbmType * binnedData,
   const FloatEbmType * targets,
   const FloatEbmType * predictorScores,
   const FloatEbmType * optionalTempParams
) {
   LOG_N(TraceLevelInfo, "Entered InitializeInteractionRegression: countFeatures=%" IntEbmTypePrintf ", features=%p, countInstances=%" IntEbmTypePrintf 
      ", binnedData=%p, targets=%p, predictorScores=%p, optionalTempParams=%p",
      countFeatures, 
      static_cast<const void *>(features), 
      countInstances, 
      static_cast<const void *>(binnedData), 
      static_cast<const void *>(targets), 
      static_cast<const void *>(predictorScores),
      static_cast<const void *>(optionalTempParams)
   );
   PEbmInteraction pEbmInteraction = reinterpret_cast<PEbmInteraction>(AllocateInteraction(
      countFeatures, 
      features, 
      k_Regression, 
      countInstances, 
      targets, 
      binnedData, 
      predictorScores,
      optionalTempParams
   ));
   LOG_N(TraceLevelInfo, "Exited InitializeInteractionRegression %p", static_cast<void *>(pEbmInteraction));
   return pEbmInteraction;
}

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses>
static IntEbmType GetInteractionScorePerTargetClasses(
   EbmInteractionState * const pEbmInteractionState, 
   const FeatureCombination * const pFeatureCombination, 
   const size_t cInstancesRequiredForChildSplitMin, 
   FloatEbmType * const pInteractionScoreReturn
) {
   // TODO : be smarter about our CachedInteractionThreadResources, otherwise why have it?
   CachedInteractionThreadResources * const pCachedThreadResources = new (std::nothrow) CachedInteractionThreadResources();
   if(nullptr == pCachedThreadResources) {
      return 1;
   }

   if(CalculateInteractionScore<compilerLearningTypeOrCountTargetClasses, 0>(
      pEbmInteractionState->m_runtimeLearningTypeOrCountTargetClasses, 
      pCachedThreadResources, 
      pEbmInteractionState->m_pDataSet, 
      pFeatureCombination, 
      cInstancesRequiredForChildSplitMin, 
      pInteractionScoreReturn
   )) {
      delete pCachedThreadResources;
      return 1;
   }
   delete pCachedThreadResources;
   return 0;
}

template<ptrdiff_t possibleCompilerLearningTypeOrCountTargetClasses>
EBM_INLINE IntEbmType CompilerRecursiveGetInteractionScore(
   const ptrdiff_t runtimeLearningTypeOrCountTargetClasses, 
   EbmInteractionState * const pEbmInteractionState, 
   const FeatureCombination * const pFeatureCombination, 
   const size_t cInstancesRequiredForChildSplitMin, 
   FloatEbmType * const pInteractionScoreReturn
) {
   static_assert(IsClassification(possibleCompilerLearningTypeOrCountTargetClasses), 
      "possibleCompilerLearningTypeOrCountTargetClasses needs to be a classification");
   EBM_ASSERT(IsClassification(runtimeLearningTypeOrCountTargetClasses));
   if(runtimeLearningTypeOrCountTargetClasses == possibleCompilerLearningTypeOrCountTargetClasses) {
      EBM_ASSERT(runtimeLearningTypeOrCountTargetClasses <= k_cCompilerOptimizedTargetClassesMax);
      return GetInteractionScorePerTargetClasses<possibleCompilerLearningTypeOrCountTargetClasses>(
         pEbmInteractionState, 
         pFeatureCombination, 
         cInstancesRequiredForChildSplitMin, 
         pInteractionScoreReturn
      );
   } else {
      return CompilerRecursiveGetInteractionScore<possibleCompilerLearningTypeOrCountTargetClasses + 1>(
         runtimeLearningTypeOrCountTargetClasses, 
         pEbmInteractionState, 
         pFeatureCombination, 
         cInstancesRequiredForChildSplitMin, 
         pInteractionScoreReturn
      );
   }
}

template<>
EBM_INLINE IntEbmType CompilerRecursiveGetInteractionScore<k_cCompilerOptimizedTargetClassesMax + 1>(
   const ptrdiff_t runtimeLearningTypeOrCountTargetClasses, 
   EbmInteractionState * const pEbmInteractionState, 
   const FeatureCombination * const pFeatureCombination, 
   const size_t cInstancesRequiredForChildSplitMin, 
   FloatEbmType * const pInteractionScoreReturn
) {
   UNUSED(runtimeLearningTypeOrCountTargetClasses);
   // it is logically possible, but uninteresting to have a classification with 1 target class, 
   // so let our runtime system handle those unlikley and uninteresting cases
   static_assert(IsClassification(k_cCompilerOptimizedTargetClassesMax), "k_cCompilerOptimizedTargetClassesMax needs to be a classification");
   EBM_ASSERT(IsClassification(runtimeLearningTypeOrCountTargetClasses));
   EBM_ASSERT(k_cCompilerOptimizedTargetClassesMax < runtimeLearningTypeOrCountTargetClasses);
   return GetInteractionScorePerTargetClasses<k_DynamicClassification>(
      pEbmInteractionState, 
      pFeatureCombination, 
      cInstancesRequiredForChildSplitMin, 
      pInteractionScoreReturn
   );
}

// we made this a global because if we had put this variable inside the EbmInteractionState object, then we would need to dereference that before getting 
// the count.  By making this global we can send a log message incase a bad EbmInteractionState object is sent into us we only decrease the count if the 
// count is non-zero, so at worst if there is a race condition then we'll output this log message more times than desired, but we can live with that
static unsigned int g_cLogGetInteractionScoreParametersMessages = 10;

EBM_NATIVE_IMPORT_EXPORT_BODY IntEbmType EBM_NATIVE_CALLING_CONVENTION GetInteractionScore(
   PEbmInteraction ebmInteraction,
   IntEbmType countFeaturesInCombination,
   const IntEbmType * featureIndexes,
   FloatEbmType * interactionScoreReturn
) {
   LOG_COUNTED_N(
      &g_cLogGetInteractionScoreParametersMessages, 
      TraceLevelInfo, 
      TraceLevelVerbose, 
      "GetInteractionScore parameters: ebmInteraction=%p, countFeaturesInCombination=%" IntEbmTypePrintf ", featureIndexes=%p, interactionScoreReturn=%p", 
      static_cast<void *>(ebmInteraction), 
      countFeaturesInCombination, 
      static_cast<const void *>(featureIndexes), 
      static_cast<void *>(interactionScoreReturn)
   );

   EBM_ASSERT(nullptr != ebmInteraction);
   EbmInteractionState * pEbmInteractionState = reinterpret_cast<EbmInteractionState *>(ebmInteraction);

   LOG_COUNTED_0(&pEbmInteractionState->m_cLogEnterMessages, TraceLevelInfo, TraceLevelVerbose, "Entered GetInteractionScore");

   EBM_ASSERT(0 <= countFeaturesInCombination);
   EBM_ASSERT(0 == countFeaturesInCombination || nullptr != featureIndexes);
   // interactionScoreReturn can be nullptr

   if(!IsNumberConvertable<size_t, IntEbmType>(countFeaturesInCombination)) {
      LOG_0(TraceLevelWarning, "WARNING GetInteractionScore !IsNumberConvertable<size_t, IntEbmType>(countFeaturesInCombination)");
      return 1;
   }
   size_t cFeaturesInCombination = static_cast<size_t>(countFeaturesInCombination);
   if(0 == cFeaturesInCombination) {
      LOG_0(TraceLevelInfo, "INFO GetInteractionScore empty feature combination");
      if(nullptr != interactionScoreReturn) {
         // we return the lowest value possible for the interaction score, but we don't return an error since we handle it even though we'd prefer our 
         // caler be smarter about this condition
         *interactionScoreReturn = 0;
      }
      return 0;
   }

   if(nullptr == pEbmInteractionState->m_pDataSet) {
      // if pEbmInteractionState->m_pDataSet is null, then we have a dataset with zero instances.  If there are zero data cases, there isn't much basis to
      // say whether there are interactions, so just return zero
      LOG_0(TraceLevelInfo, "INFO GetInteractionScore zero instances");
      if(nullptr != interactionScoreReturn) {
         // we return the lowest value possible for the interaction score, but we don't return an error since we handle it even though we'd prefer our 
         // caler be smarter about this condition
         *interactionScoreReturn = 0;
      }
      return 0;
   }

   const Feature * const aFeatures = pEbmInteractionState->m_aFeatures;
   const IntEbmType * pFeatureCombinationIndex = featureIndexes;
   const IntEbmType * const pFeatureCombinationIndexEnd = featureIndexes + cFeaturesInCombination;

   do {
      const IntEbmType indexFeatureInterop = *pFeatureCombinationIndex;
      EBM_ASSERT(0 <= indexFeatureInterop);
      if(!IsNumberConvertable<size_t, IntEbmType>(indexFeatureInterop)) {
         LOG_0(TraceLevelWarning, "WARNING GetInteractionScore !IsNumberConvertable<size_t, IntEbmType>(indexFeatureInterop)");
         return 1;
      }
      size_t iFeatureForCombination = static_cast<size_t>(indexFeatureInterop);
      EBM_ASSERT(iFeatureForCombination < pEbmInteractionState->m_cFeatures);
      const Feature * const pFeature = &aFeatures[iFeatureForCombination];
      if(pFeature->m_cBins <= 1) {
         LOG_0(TraceLevelInfo, "INFO GetInteractionScore feature with 0/1 value");
         if(nullptr != interactionScoreReturn) {
            // we return the lowest value possible for the interaction score, but we don't return an error since we handle it even though we'd prefer 
            // our caler be smarter about this condition
            *interactionScoreReturn = 0;
         }
         return 0;
      }
      ++pFeatureCombinationIndex;
   } while(pFeatureCombinationIndexEnd != pFeatureCombinationIndex);

   if(k_cDimensionsMax < cFeaturesInCombination) {
      // if we try to run with more than k_cDimensionsMax we'll exceed our memory capacity, so let's exit here instead
      LOG_0(TraceLevelWarning, "WARNING GetInteractionScore k_cDimensionsMax < cFeaturesInCombination");
      return 1;
   }

   // put the pFeatureCombination object on the stack. We want to put it into a FeatureCombination object since we want to share code with boosting, 
   // which calls things like building the tensor totals (which is templated to be compiled many times)
   char FeatureCombinationBuffer[k_cBytesFeatureCombinationMax];
   FeatureCombination * const pFeatureCombination = reinterpret_cast<FeatureCombination *>(&FeatureCombinationBuffer);
   pFeatureCombination->Initialize(cFeaturesInCombination, 0);

   pFeatureCombinationIndex = featureIndexes; // restart from the start
   FeatureCombination::FeatureCombinationEntry * pFeatureCombinationEntry = ARRAY_TO_POINTER(pFeatureCombination->m_FeatureCombinationEntry);
   do {
      const IntEbmType indexFeatureInterop = *pFeatureCombinationIndex;
      EBM_ASSERT(0 <= indexFeatureInterop);
      EBM_ASSERT((IsNumberConvertable<size_t, IntEbmType>(indexFeatureInterop))); // we already checked indexFeatureInterop was good above
      size_t iFeatureForCombination = static_cast<size_t>(indexFeatureInterop);
      EBM_ASSERT(iFeatureForCombination < pEbmInteractionState->m_cFeatures);
      const Feature * const pFeature = &aFeatures[iFeatureForCombination];
      EBM_ASSERT(2 <= pFeature->m_cBins); // we should have filtered out anything with 1 bin above

      pFeatureCombinationEntry->m_pFeature = pFeature;
      ++pFeatureCombinationEntry;
      ++pFeatureCombinationIndex;
   } while(pFeatureCombinationIndexEnd != pFeatureCombinationIndex);

   IntEbmType ret;
   if(IsClassification(pEbmInteractionState->m_runtimeLearningTypeOrCountTargetClasses)) {
      if(pEbmInteractionState->m_runtimeLearningTypeOrCountTargetClasses <= ptrdiff_t { 1 }) {
         LOG_0(TraceLevelInfo, "INFO GetInteractionScore target with 0/1 classes");
         if(nullptr != interactionScoreReturn) {
            // if there is only 1 classification target, then we can predict the outcome with 100% accuracy and there is no need for logits or 
            // interactions or anything else.  We return 0 since interactions have no benefit
            *interactionScoreReturn = FloatEbmType { 0 };
         }
         return 0;
      }
      ret = CompilerRecursiveGetInteractionScore<2>(
         pEbmInteractionState->m_runtimeLearningTypeOrCountTargetClasses, 
         pEbmInteractionState, 
         pFeatureCombination, 
         TODO_REMOVE_THIS_DEFAULT_cInstancesRequiredForChildSplitMin, 
         interactionScoreReturn
      );
   } else {
      EBM_ASSERT(IsRegression(pEbmInteractionState->m_runtimeLearningTypeOrCountTargetClasses));
      ret = GetInteractionScorePerTargetClasses<k_Regression>(
         pEbmInteractionState, 
         pFeatureCombination, 
         TODO_REMOVE_THIS_DEFAULT_cInstancesRequiredForChildSplitMin, 
         interactionScoreReturn
      );
   }
   if(0 != ret) {
      LOG_N(TraceLevelWarning, "WARNING GetInteractionScore returned %" IntEbmTypePrintf, ret);
   }
   if(nullptr != interactionScoreReturn) {
      // if *interactionScoreReturn was negative for floating point instability reasons, we zero it so that we don't return a negative number to our caller
      EBM_ASSERT(FloatEbmType { 0 } <= *interactionScoreReturn);
      LOG_COUNTED_N(
         &pEbmInteractionState->m_cLogExitMessages, 
         TraceLevelInfo, 
         TraceLevelVerbose, 
         "Exited GetInteractionScore %" FloatEbmTypePrintf, *interactionScoreReturn
      );
   } else {
      LOG_COUNTED_0(&pEbmInteractionState->m_cLogExitMessages, TraceLevelInfo, TraceLevelVerbose, "Exited GetInteractionScore");
   }
   return ret;
}

EBM_NATIVE_IMPORT_EXPORT_BODY void EBM_NATIVE_CALLING_CONVENTION FreeInteraction(
   PEbmInteraction ebmInteraction
) {
   LOG_N(TraceLevelInfo, "Entered FreeInteraction: ebmInteraction=%p", static_cast<void *>(ebmInteraction));
   EbmInteractionState * pEbmInteractionState = reinterpret_cast<EbmInteractionState *>(ebmInteraction);
   // pEbmInteractionState == nullptr is legal, just like delete/free
   delete pEbmInteractionState;
   LOG_0(TraceLevelInfo, "Exited FreeInteraction");
}
