#pragma once

#include "PhysicsInstance.hpp"
#include "ThreadPool.hpp"
#include "Simulation/Controller.hpp"

namespace phylo {
	class Simulation;
	namespace Physics {
		class Controller final : public SparseComponentController<Physics::Instance> {
			static constexpr const bool SINGLE_THREADED = false;
			static constexpr const usize GridArraySize = 16;

			using instance_t = Physics::Instance;

			void pool_update(usize threadID) __restrict;
			void pool_update2(usize threadID) __restrict;

			template <uint32 elements>
			struct InstanceSubArray {
				uint32                          m_ElementCount = 0u;
				//array<instance_t *, elements>   m_Elements;
				array<instance_t * >             m_Elements;
				mutex                           m_Lock;

				InstanceSubArray() = default;
				InstanceSubArray(const InstanceSubArray & __restrict) {
					m_Elements.reserve(16);
				}

				void removeElement(instance_t * __restrict instance) __restrict {
					m_Lock.lock();
					uint32 idx = instance->m_GridIndex;
					xassert(m_Elements[idx] == instance, "Instance Mismatch");

					xassert(m_ElementCount != 0, "Cannot remove non-existent element");

					if constexpr (options::Deterministic) {
						for (uint i = idx + 1; i < m_ElementCount; ++i) {
							--m_Elements[i]->m_GridIndex;
						}
						if ((m_ElementCount > 1) & (idx != (m_ElementCount - 1))) {
							memmove(&m_Elements[idx], &m_Elements[idx + 1], (m_ElementCount - (idx + 1)) * sizeof(instance_t *));
						}

						--m_ElementCount;
					}
					else {
						--m_ElementCount;
						m_Elements[idx] = m_Elements[m_ElementCount];
						m_Elements[idx]->m_GridIndex = idx;
					}

					m_Lock.unlock();
				}

				void addElement(instance_t * __restrict instance) __restrict {
					m_Lock.lock();
					//xassert(m_ElementCount < elements, "InstanceSubArray element overflow");

					// Add the element so that all elements are in order.

					if constexpr (options::Deterministic) {
						uint32 elementCount = m_ElementCount++;
						m_Elements.resize(elementCount + 1, nullptr);

						uint i = 0;
						bool elemFound = false;

						for (; i < elementCount; ++i) {
							if (uptr(instance) < uptr(m_Elements[i])) {
								// Then insert it here.
								memcpy(&m_Elements[i + 1], &m_Elements[i], (elementCount - i) * sizeof(instance_t *));
								m_Elements[i] = instance;
								instance->m_GridIndex = i;
								elemFound = true;
								break;
							}
						}

						// If we are here, we are putting it at the end.
						if (!elemFound) {
							m_Elements[elementCount] = instance;
							instance->m_GridIndex = elementCount;
						}
						else {
							for (++i; i < elementCount + 1; ++i) {
								m_Elements[i]->m_GridIndex = i;
							}
						}
					}
					else {
						uint32 idx = m_ElementCount++;
						m_Elements.resize(idx + 1, nullptr);
						m_Elements[idx] = instance;
						instance->m_GridIndex = idx;
					}


					m_Lock.unlock();
				}
			};

			array<InstanceSubArray<GridArraySize>>      m_GridElements;

			Simulation& m_Simulation;

			ThreadPool								m_ThreadPool;
			ThreadPool								m_ThreadPool2;
			atomic<uint>							m_ThreadPoolIndex;

			float                                      m_GridElementSize;
			float                                      m_GridElementSizeHalf;
			float                                      m_InvGridElementSize;
			uint32                                      m_GridElementsEdge;

			vector2F GetGridOffset(uint gridElement) const __restrict;
			vector2F GetGridXRange(uint gridElement) const __restrict;
			vector2F GetGridXRangeFromX(uint gridElement) const __restrict;
			vector2F GetGridYRangeFromY(uint gridElement) const __restrict;
			uint32 GetInstanceOffset(const instance_t & __restrict instance) const __restrict;
			uint32 GetPositionOffset(const vector2F & __restrict position) const __restrict;
			uint32 GetInstanceOffset(uint x, uint y) const __restrict;

			virtual void removedInstance(instance_t * __restrict instance) __restrict override final;
			virtual void insertedInstance(instance_t * __restrict instance) __restrict override final;

		public:
			Controller() = delete;
			explicit Controller(Simulation &simulation);
			~Controller() override;

			void update() ;

			Cell *findCell(const vector2F & __restrict position, float radius, const Cell * __restrict filter) const __restrict;
		};
	}
}
