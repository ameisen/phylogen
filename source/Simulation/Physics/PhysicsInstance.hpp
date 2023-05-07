#pragma once

namespace phylo {
	class Cell;
	namespace Physics {
		class Controller;
		struct Instance {
			vector2F		 m_Position;
			vector2F		 m_ShadowPosition;
			vector2F		 m_Direction = { 1.0f, 0.0f };
			vector2F		 m_Velocity;
			vector2F		m_ShadowVelocity;
			Cell			 *m_Cell = nullptr;
			float		 m_Radius;
			float		 m_ShadowRadius;
			uint			 m_TouchedThisFrame = 0;
			atomic<uint>	 m_ShadowLock = 0;

			Instance(const Instance & __restrict inst) :
				m_Position(inst.m_Position),
				m_Direction(inst.m_Direction),
				m_Velocity(inst.m_Velocity),
				m_Cell(inst.m_Cell),
				m_Radius(inst.m_Radius),
				m_ShadowRadius(inst.m_Radius),
				m_TouchedThisFrame(inst.m_TouchedThisFrame),
				m_ShadowLock(inst.m_ShadowLock.load())
			{}

			Instance() = default;

			Instance & operator = (const Instance & __restrict src) __restrict {
				m_Position = src.m_Position;
				m_Direction = src.m_Direction;
				m_Velocity = src.m_Velocity;
				m_Radius = src.m_Radius;
				m_ShadowRadius = src.m_Radius;
				return *this;
			}

			uint32		m_GridArrayIndex = uint32(-1);
			uint32		m_GridIndex = uint32(-1);

			bool		m_Valid = false;

			void unserialize(Stream &inStream, Cell *cell) ;
			void serialize(Stream &outStream) const ;
		};
	}
}
