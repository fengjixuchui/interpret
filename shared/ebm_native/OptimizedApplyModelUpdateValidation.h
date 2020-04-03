// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <ebm@koch.ninja>

#ifndef OPTIMIZED_APPLY_MODEL_UPDATE_VALIDATION_H
#define OPTIMIZED_APPLY_MODEL_UPDATE_VALIDATION_H

#include <stddef.h> // size_t, ptrdiff_t

#include "ebm_native.h"
#include "EbmInternal.h"
// very independent includes
#include "Logging.h" // EBM_ASSERT & LOG
#include "EbmStatistics.h"
// FeatureCombination.h depends on FeatureInternal.h
#include "FeatureCombination.h"
// dataset depends on features
#include "DataSetByFeatureCombination.h"

// C++ does not allow partial function specialization, so we need to use these cumbersome static class functions to do partial function specialization

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses>
class OptimizedApplyModelUpdateValidationZeroFeatures {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      EBM_ASSERT(IsClassification(compilerLearningTypeOrCountTargetClasses));
      EBM_ASSERT(!IsBinaryClassification(compilerLearningTypeOrCountTargetClasses));

      const ptrdiff_t learningTypeOrCountTargetClasses = GET_LEARNING_TYPE_OR_COUNT_TARGET_CLASSES(
         compilerLearningTypeOrCountTargetClasses,
         runtimeLearningTypeOrCountTargetClasses
      );
      const size_t cVectorLength = GetVectorLength(learningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);

      FloatEbmType sumLogLoss = FloatEbmType { 0 };
      const StorageDataType * pTargetData = pValidationSet->GetTargetDataPointer();
      FloatEbmType * pPredictorScores = pValidationSet->GetPredictorScores();
      const FloatEbmType * const pPredictorScoresEnd = pPredictorScores + cInstances * cVectorLength;
      do {
         size_t targetData = static_cast<size_t>(*pTargetData);
         ++pTargetData;


         const FloatEbmType * pValues = aModelFeatureCombinationUpdateTensor;
         FloatEbmType itemExp = FloatEbmType { 0 };
         FloatEbmType sumExp = FloatEbmType { 0 };
         size_t iVector = 0;
         do {
            const FloatEbmType smallChangeToPredictorScores = *pValues;
            ++pValues;
            // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
            const FloatEbmType predictorScore = *pPredictorScores + smallChangeToPredictorScores;
            *pPredictorScores = predictorScore;
            ++pPredictorScores;
            const FloatEbmType oneExp = EbmExp(predictorScore);
            itemExp = iVector == targetData ? oneExp : itemExp;
            sumExp += oneExp;
            ++iVector;
         } while(iVector < cVectorLength);
         // TODO: store the result of std::exp above for the index that we care about above since exp(..) is going to be expensive and probably 
         // even more expensive than an unconditional branch
         const FloatEbmType instanceLogLoss = EbmStatistics::ComputeSingleInstanceLogLossMulticlass(
            sumExp,
            itemExp
         );

         EBM_ASSERT(std::isnan(instanceLogLoss) || -k_epsilonLogLoss <= instanceLogLoss);
         sumLogLoss += instanceLogLoss;

      } while(pPredictorScoresEnd != pPredictorScores);
      return sumLogLoss / cInstances;
   }
};

#ifndef EXPAND_BINARY_LOGITS
template<>
class OptimizedApplyModelUpdateValidationZeroFeatures<2> {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      UNUSED(runtimeLearningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);

      FloatEbmType sumLogLoss = 0;
      const StorageDataType * pTargetData = pValidationSet->GetTargetDataPointer();
      FloatEbmType * pPredictorScores = pValidationSet->GetPredictorScores();
      const FloatEbmType * const pPredictorScoresEnd = pPredictorScores + cInstances;
      const FloatEbmType smallChangeToPredictorScores = aModelFeatureCombinationUpdateTensor[0];
      do {
         size_t targetData = static_cast<size_t>(*pTargetData);
         ++pTargetData;
         // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
         const FloatEbmType predictorScore = *pPredictorScores + smallChangeToPredictorScores;
         *pPredictorScores = predictorScore;
         ++pPredictorScores;
         const FloatEbmType instanceLogLoss = EbmStatistics::ComputeSingleInstanceLogLossBinaryClassification(predictorScore, targetData);
         EBM_ASSERT(std::isnan(instanceLogLoss) || FloatEbmType { 0 } <= instanceLogLoss);
         sumLogLoss += instanceLogLoss;
      } while(pPredictorScoresEnd != pPredictorScores);
      return sumLogLoss / cInstances;
   }
};
#endif // EXPAND_BINARY_LOGITS

template<>
class OptimizedApplyModelUpdateValidationZeroFeatures<k_Regression> {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      UNUSED(runtimeLearningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);

      FloatEbmType sumSquareError = FloatEbmType { 0 };
      FloatEbmType * pResidualError = pValidationSet->GetResidualPointer();
      const FloatEbmType * const pResidualErrorEnd = pResidualError + cInstances;
      const FloatEbmType smallChangeToPrediction = aModelFeatureCombinationUpdateTensor[0];
      do {
         // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
         const FloatEbmType residualError = EbmStatistics::ComputeResidualErrorRegression(*pResidualError - smallChangeToPrediction);
         const FloatEbmType instanceSquaredError = EbmStatistics::ComputeSingleInstanceSquaredErrorRegression(residualError);
         EBM_ASSERT(std::isnan(instanceSquaredError) || FloatEbmType { 0 } <= instanceSquaredError);
         sumSquareError += instanceSquaredError;
         *pResidualError = residualError;
         ++pResidualError;
      } while(pResidualErrorEnd != pResidualError);
      return sumSquareError / cInstances;
   }
};

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses, size_t compilerCountItemsPerBitPackedDataUnit>
class OptimizedApplyModelUpdateValidationInternal {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      const size_t runtimeCountItemsPerBitPackedDataUnit,
      const FeatureCombination * const pFeatureCombination,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      EBM_ASSERT(IsClassification(compilerLearningTypeOrCountTargetClasses));
      EBM_ASSERT(!IsBinaryClassification(compilerLearningTypeOrCountTargetClasses));

      const ptrdiff_t learningTypeOrCountTargetClasses = GET_LEARNING_TYPE_OR_COUNT_TARGET_CLASSES(
         compilerLearningTypeOrCountTargetClasses,
         runtimeLearningTypeOrCountTargetClasses
      );
      const size_t cVectorLength = GetVectorLength(learningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);
      EBM_ASSERT(0 < pFeatureCombination->m_cFeatures);

      const size_t cItemsPerBitPackedDataUnit = GET_COUNT_ITEMS_PER_BIT_PACKED_DATA_UNIT(
         compilerCountItemsPerBitPackedDataUnit,
         runtimeCountItemsPerBitPackedDataUnit
      );
      EBM_ASSERT(1 <= cItemsPerBitPackedDataUnit);
      EBM_ASSERT(cItemsPerBitPackedDataUnit <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPackedDataUnit);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const size_t maskBits = std::numeric_limits<size_t>::max() >> (k_cBitsForStorageType - cBitsPerItemMax);

      FloatEbmType sumLogLoss = FloatEbmType { 0 };
      const StorageDataType * pInputData = pValidationSet->GetInputDataPointer(pFeatureCombination);
      const StorageDataType * pTargetData = pValidationSet->GetTargetDataPointer();
      FloatEbmType * pPredictorScores = pValidationSet->GetPredictorScores();

      // this shouldn't overflow since we're accessing existing memory
      const FloatEbmType * const pPredictorScoresTrueEnd = pPredictorScores + cInstances * cVectorLength;
      const FloatEbmType * pPredictorScoresExit = pPredictorScoresTrueEnd;
      const FloatEbmType * pPredictorScoresInnerEnd = pPredictorScoresTrueEnd;
      if(cInstances <= cItemsPerBitPackedDataUnit) {
         goto one_last_loop;
      }
      pPredictorScoresExit = pPredictorScoresTrueEnd - ((cInstances - 1) % cItemsPerBitPackedDataUnit + 1) * cVectorLength;
      EBM_ASSERT(pPredictorScores < pPredictorScoresExit);
      EBM_ASSERT(pPredictorScoresExit < pPredictorScoresTrueEnd);

      do {
         pPredictorScoresInnerEnd = pPredictorScores + cItemsPerBitPackedDataUnit * cVectorLength;
         // jumping back into this loop and changing pPredictorScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPackedDataUnit, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         size_t iTensorBinCombined = static_cast<size_t>(*pInputData);
         ++pInputData;
         do {
            size_t targetData = static_cast<size_t>(*pTargetData);
            ++pTargetData;

            const size_t iTensorBin = maskBits & iTensorBinCombined;
            const FloatEbmType * pValues = &aModelFeatureCombinationUpdateTensor[iTensorBin * cVectorLength];
            FloatEbmType itemExp = FloatEbmType { 0 };
            FloatEbmType sumExp = FloatEbmType { 0 };
            size_t iVector = 0;
            do {
               const FloatEbmType smallChangeToPredictorScores = *pValues;
               ++pValues;
               // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
               const FloatEbmType predictorScore = *pPredictorScores + smallChangeToPredictorScores;
               *pPredictorScores = predictorScore;
               ++pPredictorScores;
               const FloatEbmType oneExp = EbmExp(predictorScore);
               itemExp = iVector == targetData ? oneExp : itemExp;
               sumExp += oneExp;
               ++iVector;
            } while(iVector < cVectorLength);
            // TODO: store the result of std::exp above for the index that we care about above since exp(..) is going to be expensive and 
            // probably even more expensive than an unconditional branch
            const FloatEbmType instanceLogLoss = EbmStatistics::ComputeSingleInstanceLogLossMulticlass(
               sumExp,
               itemExp
            );

            EBM_ASSERT(std::isnan(instanceLogLoss) || -k_epsilonLogLoss <= instanceLogLoss);
            sumLogLoss += instanceLogLoss;
            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pPredictorScoresInnerEnd != pPredictorScores);
      } while(pPredictorScoresExit != pPredictorScores);

      // first time through?
      if(pPredictorScoresTrueEnd != pPredictorScores) {
         pPredictorScoresInnerEnd = pPredictorScoresTrueEnd;
         pPredictorScoresExit = pPredictorScoresTrueEnd;
         goto one_last_loop;
      }
      return sumLogLoss / cInstances;
   }
};

#ifndef EXPAND_BINARY_LOGITS
template<size_t compilerCountItemsPerBitPackedDataUnit>
class OptimizedApplyModelUpdateValidationInternal<2, compilerCountItemsPerBitPackedDataUnit> {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      const size_t runtimeCountItemsPerBitPackedDataUnit,
      const FeatureCombination * const pFeatureCombination,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      UNUSED(runtimeLearningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);
      EBM_ASSERT(0 < pFeatureCombination->m_cFeatures);

      const size_t cItemsPerBitPackedDataUnit = GET_COUNT_ITEMS_PER_BIT_PACKED_DATA_UNIT(
         compilerCountItemsPerBitPackedDataUnit,
         runtimeCountItemsPerBitPackedDataUnit
      );
      EBM_ASSERT(1 <= cItemsPerBitPackedDataUnit);
      EBM_ASSERT(cItemsPerBitPackedDataUnit <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPackedDataUnit);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const size_t maskBits = std::numeric_limits<size_t>::max() >> (k_cBitsForStorageType - cBitsPerItemMax);

      FloatEbmType sumLogLoss = FloatEbmType { 0 };
      const StorageDataType * pInputData = pValidationSet->GetInputDataPointer(pFeatureCombination);
      const StorageDataType * pTargetData = pValidationSet->GetTargetDataPointer();
      FloatEbmType * pPredictorScores = pValidationSet->GetPredictorScores();

      // this shouldn't overflow since we're accessing existing memory
      const FloatEbmType * const pPredictorScoresTrueEnd = pPredictorScores + cInstances;
      const FloatEbmType * pPredictorScoresExit = pPredictorScoresTrueEnd;
      const FloatEbmType * pPredictorScoresInnerEnd = pPredictorScoresTrueEnd;
      if(cInstances <= cItemsPerBitPackedDataUnit) {
         goto one_last_loop;
      }
      pPredictorScoresExit = pPredictorScoresTrueEnd - ((cInstances - 1) % cItemsPerBitPackedDataUnit + 1);
      EBM_ASSERT(pPredictorScores < pPredictorScoresExit);
      EBM_ASSERT(pPredictorScoresExit < pPredictorScoresTrueEnd);

      do {
         pPredictorScoresInnerEnd = pPredictorScores + cItemsPerBitPackedDataUnit;
         // jumping back into this loop and changing pPredictorScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPackedDataUnit, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         size_t iTensorBinCombined = static_cast<size_t>(*pInputData);
         ++pInputData;
         do {
            size_t targetData = static_cast<size_t>(*pTargetData);
            ++pTargetData;

            const size_t iTensorBin = maskBits & iTensorBinCombined;

            const FloatEbmType smallChangeToPredictorScores = aModelFeatureCombinationUpdateTensor[iTensorBin];
            // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
            const FloatEbmType predictorScore = *pPredictorScores + smallChangeToPredictorScores;
            *pPredictorScores = predictorScore;
            ++pPredictorScores;
            const FloatEbmType instanceLogLoss = EbmStatistics::ComputeSingleInstanceLogLossBinaryClassification(predictorScore, targetData);

            EBM_ASSERT(std::isnan(instanceLogLoss) || FloatEbmType { 0 } <= instanceLogLoss);
            sumLogLoss += instanceLogLoss;

            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pPredictorScoresInnerEnd != pPredictorScores);
      } while(pPredictorScoresExit != pPredictorScores);

      // first time through?
      if(pPredictorScoresTrueEnd != pPredictorScores) {
         pPredictorScoresInnerEnd = pPredictorScoresTrueEnd;
         pPredictorScoresExit = pPredictorScoresTrueEnd;
         goto one_last_loop;
      }
      return sumLogLoss / cInstances;
   }
};
#endif // EXPAND_BINARY_LOGITS

template<size_t compilerCountItemsPerBitPackedDataUnit>
class OptimizedApplyModelUpdateValidationInternal<k_Regression, compilerCountItemsPerBitPackedDataUnit> {
public:
   static FloatEbmType Func(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      const size_t runtimeCountItemsPerBitPackedDataUnit,
      const FeatureCombination * const pFeatureCombination,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      UNUSED(runtimeLearningTypeOrCountTargetClasses);
      const size_t cInstances = pValidationSet->GetCountInstances();
      EBM_ASSERT(0 < cInstances);
      EBM_ASSERT(0 < pFeatureCombination->m_cFeatures);

      const size_t cItemsPerBitPackedDataUnit = GET_COUNT_ITEMS_PER_BIT_PACKED_DATA_UNIT(
         compilerCountItemsPerBitPackedDataUnit,
         runtimeCountItemsPerBitPackedDataUnit
      );
      EBM_ASSERT(1 <= cItemsPerBitPackedDataUnit);
      EBM_ASSERT(cItemsPerBitPackedDataUnit <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPackedDataUnit);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const size_t maskBits = std::numeric_limits<size_t>::max() >> (k_cBitsForStorageType - cBitsPerItemMax);

      FloatEbmType sumSquareError = FloatEbmType { 0 };
      FloatEbmType * pResidualError = pValidationSet->GetResidualPointer();
      const StorageDataType * pInputData = pValidationSet->GetInputDataPointer(pFeatureCombination);


      // this shouldn't overflow since we're accessing existing memory
      const FloatEbmType * const pResidualErrorTrueEnd = pResidualError + cInstances;
      const FloatEbmType * pResidualErrorExit = pResidualErrorTrueEnd;
      const FloatEbmType * pResidualErrorInnerEnd = pResidualErrorTrueEnd;
      if(cInstances <= cItemsPerBitPackedDataUnit) {
         goto one_last_loop;
      }
      pResidualErrorExit = pResidualErrorTrueEnd - ((cInstances - 1) % cItemsPerBitPackedDataUnit + 1);
      EBM_ASSERT(pResidualError < pResidualErrorExit);
      EBM_ASSERT(pResidualErrorExit < pResidualErrorTrueEnd);

      do {
         pResidualErrorInnerEnd = pResidualError + cItemsPerBitPackedDataUnit;
         // jumping back into this loop and changing pPredictorScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPackedDataUnit, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         size_t iTensorBinCombined = static_cast<size_t>(*pInputData);
         ++pInputData;
         do {
            const size_t iTensorBin = maskBits & iTensorBinCombined;

            const FloatEbmType smallChangeToPrediction = aModelFeatureCombinationUpdateTensor[iTensorBin];
            // this will apply a small fix to our existing ValidationPredictorScores, either positive or negative, whichever is needed
            const FloatEbmType residualError = EbmStatistics::ComputeResidualErrorRegression(*pResidualError - smallChangeToPrediction);
            const FloatEbmType instanceSquaredError = EbmStatistics::ComputeSingleInstanceSquaredErrorRegression(residualError);
            EBM_ASSERT(std::isnan(instanceSquaredError) || FloatEbmType { 0 } <= instanceSquaredError);
            sumSquareError += instanceSquaredError;
            *pResidualError = residualError;
            ++pResidualError;

            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pResidualErrorInnerEnd != pResidualError);
      } while(pResidualErrorExit != pResidualError);

      // first time through?
      if(pResidualErrorTrueEnd != pResidualError) {
         pResidualErrorInnerEnd = pResidualErrorTrueEnd;
         pResidualErrorExit = pResidualErrorTrueEnd;
         goto one_last_loop;
      }
      return sumSquareError / cInstances;
   }
};

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses, size_t compilerCountItemsPerBitPackedDataUnitPossible>
class OptimizedApplyModelUpdateValidationCompiler {
public:
   EBM_INLINE static FloatEbmType MagicCompilerLoopFunction(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      const size_t runtimeCountItemsPerBitPackedDataUnit,
      const FeatureCombination * const pFeatureCombination,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      EBM_ASSERT(1 <= runtimeCountItemsPerBitPackedDataUnit);
      EBM_ASSERT(runtimeCountItemsPerBitPackedDataUnit <= k_cBitsForStorageType);
      static_assert(compilerCountItemsPerBitPackedDataUnitPossible <= k_cBitsForStorageType, "We can't have this many items in a data pack.");
      if(compilerCountItemsPerBitPackedDataUnitPossible == runtimeCountItemsPerBitPackedDataUnit) {
         return OptimizedApplyModelUpdateValidationInternal<compilerLearningTypeOrCountTargetClasses, compilerCountItemsPerBitPackedDataUnitPossible>::Func(
            runtimeLearningTypeOrCountTargetClasses,
            runtimeCountItemsPerBitPackedDataUnit,
            pFeatureCombination,
            pValidationSet,
            aModelFeatureCombinationUpdateTensor
         );
      } else {
         return OptimizedApplyModelUpdateValidationCompiler<
            compilerLearningTypeOrCountTargetClasses,
            GetNextCountItemsBitPacked(compilerCountItemsPerBitPackedDataUnitPossible)
         >::MagicCompilerLoopFunction(
            runtimeLearningTypeOrCountTargetClasses,
            runtimeCountItemsPerBitPackedDataUnit,
            pFeatureCombination,
            pValidationSet,
            aModelFeatureCombinationUpdateTensor
         );
      }
   }
};

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses>
class OptimizedApplyModelUpdateValidationCompiler<compilerLearningTypeOrCountTargetClasses, k_cItemsPerBitPackedDataUnitDynamic> {
public:
   EBM_INLINE static FloatEbmType MagicCompilerLoopFunction(
      const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
      const size_t runtimeCountItemsPerBitPackedDataUnit,
      const FeatureCombination * const pFeatureCombination,
      DataSetByFeatureCombination * const pValidationSet,
      const FloatEbmType * const aModelFeatureCombinationUpdateTensor
   ) {
      EBM_ASSERT(1 <= runtimeCountItemsPerBitPackedDataUnit);
      EBM_ASSERT(runtimeCountItemsPerBitPackedDataUnit <= k_cBitsForStorageType);
      return OptimizedApplyModelUpdateValidationInternal<compilerLearningTypeOrCountTargetClasses, k_cItemsPerBitPackedDataUnitDynamic>::Func(
         runtimeLearningTypeOrCountTargetClasses,
         runtimeCountItemsPerBitPackedDataUnit,
         pFeatureCombination,
         pValidationSet,
         aModelFeatureCombinationUpdateTensor
      );
   }
};

template<ptrdiff_t compilerLearningTypeOrCountTargetClasses>
EBM_INLINE static FloatEbmType OptimizedApplyModelUpdateValidation(
   const ptrdiff_t runtimeLearningTypeOrCountTargetClasses,
   const bool bUseSIMD,
   const FeatureCombination * const pFeatureCombination,
   DataSetByFeatureCombination * const pValidationSet,
   const FloatEbmType * const aModelFeatureCombinationUpdateTensor
) {
   LOG_0(TraceLevelVerbose, "Entered OptimizedApplyModelUpdateValidation");

   FloatEbmType ret;
   if(0 == pFeatureCombination->m_cFeatures) {
      ret = OptimizedApplyModelUpdateValidationZeroFeatures<compilerLearningTypeOrCountTargetClasses>::Func(
         runtimeLearningTypeOrCountTargetClasses,
         pValidationSet,
         aModelFeatureCombinationUpdateTensor
      );
   } else {
      if(bUseSIMD) {
         // TODO : enable SIMD(AVX-512) to work

         // 64 - do 8 at a time and unroll the loop 8 times.  These are bool features and are common.  Put the unrolled inner loop into a function
         // 32 - do 8 at a time and unroll the loop 4 times.  These are bool features and are common.  Put the unrolled inner loop into a function
         // 21 - do 8 at a time and unroll the loop 3 times (ignore the last 3 with a mask)
         // 16 - do 8 at a time and unroll the loop 2 times.  These are bool features and are common.  Put the unrolled inner loop into a function
         // 12 - do 8 of them, shift the low 4 upwards and then load the next 12 and take the top 4, repeat.
         // 10 - just drop this down to packing 8 together
         // 9 - just drop this down to packing 8 together
         // 8 - do all 8 at a time without an inner loop.  This is one of the most common values.  256 binned values
         // 7,6,5,4,3,2,1 - use a mask to exclude the non-used conditions and process them like the 8.  These are rare since they require more than 256 values

         ret = OptimizedApplyModelUpdateValidationCompiler<
            compilerLearningTypeOrCountTargetClasses,
            k_cItemsPerBitPackedDataUnitMax
         >::MagicCompilerLoopFunction(
            runtimeLearningTypeOrCountTargetClasses,
            pFeatureCombination->m_cItemsPerBitPackedDataUnit,
            pFeatureCombination,
            pValidationSet,
            aModelFeatureCombinationUpdateTensor
         );
      } else {
         // there isn't much benefit in eliminating the loop that unpacks a data unit unless we're also unpacking that to SIMD code
         // Our default packing structure is to bin continuous values to 256 values, and we have 64 bit packing structures, so we usually
         // have more than 8 values per memory fetch.  Eliminating the inner loop for multiclass is valuable since we can have low numbers like 3 class,
         // 4 class, etc, but by the time we get to 8 loops with exp inside and a lot of other instructures we should worry that our code expansion
         // will exceed the L1 instruction cache size.  With SIMD we do 8 times the work in the same number of instructions so these are lesser issues
         ret = OptimizedApplyModelUpdateValidationInternal<compilerLearningTypeOrCountTargetClasses, k_cItemsPerBitPackedDataUnitDynamic>::Func(
            runtimeLearningTypeOrCountTargetClasses,
            pFeatureCombination->m_cItemsPerBitPackedDataUnit,
            pFeatureCombination,
            pValidationSet,
            aModelFeatureCombinationUpdateTensor
         );
      }
   }

   EBM_ASSERT(std::isnan(ret) || -k_epsilonLogLoss <= ret);
   if(UNLIKELY(UNLIKELY(std::isnan(ret)) || UNLIKELY(std::isinf(ret)))) {
      // set the metric so high that this round of boosting will be rejected.  The worst metric is std::numeric_limits<FloatEbmType>::max(),
      // Set it to that so that this round of boosting won't be accepted if our caller is using early stopping
      ret = std::numeric_limits<FloatEbmType>::max();
   } else {
      if(IsClassification(compilerLearningTypeOrCountTargetClasses)) {
         if(UNLIKELY(ret < FloatEbmType { 0 })) {
            // regression can't be negative since squares are pretty well insulated from ever doing that

            // Multiclass can return small negative numbers, so we need to clean up the value retunred so that it isn't negative

            // binary classification can't return a negative number provided the log function
            // doesn't ever return a negative number for numbers exactly equal to 1 or higher
            // BUT we're going to be using or trying approximate log functions, and those might not
            // be guaranteed to return a positive or zero number, so let's just always check for numbers less than zero and round up
            EBM_ASSERT(IsMulticlass(compilerLearningTypeOrCountTargetClasses));

            // because of floating point inexact reasons, ComputeSingleInstanceLogLossMulticlass can return a negative number
            // so correct this before we return.  Any negative numbers were really meant to be zero
            ret = FloatEbmType { 0 };
         }
      }
   }
   EBM_ASSERT(!std::isnan(ret));
   EBM_ASSERT(!std::isinf(ret));
   EBM_ASSERT(FloatEbmType { 0 } <= ret);

   LOG_0(TraceLevelVerbose, "Exited OptimizedApplyModelUpdateValidation");

   return ret;
}

#endif // OPTIMIZED_APPLY_MODEL_UPDATE_VALIDATION_H
