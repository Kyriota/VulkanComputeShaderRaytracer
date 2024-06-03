#include "fluid_particle_system.hpp"
#include "lve/lve_math.hpp"
#include "lve/lve_file_io.hpp"
#include "lve/lve_math.hpp"

// std
#include <algorithm>
#include <iostream>

namespace lve
{
    FluidParticleSystem::FluidParticleSystem(const std::string &configFilePath, VkExtent2D windowExtent) : windowExtent(windowExtent)
    {
        this->configFilePath = configFilePath;
        LveYamlConfig config{configFilePath};

        particleCount = config.get<unsigned int>("particleCount");
        smoothRadius = config.get<float>("smoothRadius");
        collisionDamping = config.get<float>("collisionDamping");
        targetDensity = config.get<float>("targetDensity");
        pressureMultiplier = config.get<float>("pressureMultiplier");
        gravityAccValue = config.get<float>("gravityAccValue");

        std::vector<float> startPoint = config.get<std::vector<float>>("startPoint");
        float stride = config.get<float>("stride");
        float maxWidth = config.get<float>("maxWidth");
        bool randomize = config.get<bool>("randomize");

        // constants
        scalingFactorPoly6_2D = 4.f / (M_PI * LveMath::intPow(smoothRadius, 8));
        scalingFactorSpikyPow3_2D = 10.f / (M_PI * LveMath::intPow(smoothRadius, 5));
        scalingFactorSpikyPow2_2D = 6.f / (M_PI * LveMath::intPow(smoothRadius, 4));

        initParticleData(glm::vec2(startPoint[0], startPoint[1]), stride, maxWidth, randomize);
    }

    void FluidParticleSystem::reloadConfigParam()
    {
        LveYamlConfig config{configFilePath};

        smoothRadius = config.get<float>("smoothRadius");
        collisionDamping = config.get<float>("collisionDamping");
        targetDensity = config.get<float>("targetDensity");
        pressureMultiplier = config.get<float>("pressureMultiplier");
        gravityAccValue = config.get<float>("gravityAccValue");

        // constants
        scalingFactorPoly6_2D = 4.f / (M_PI * LveMath::intPow(smoothRadius, 8));
        scalingFactorSpikyPow3_2D = 10.f / (M_PI * LveMath::intPow(smoothRadius, 5));
        scalingFactorSpikyPow2_2D = 6.f / (M_PI * LveMath::intPow(smoothRadius, 4));
    }

    void FluidParticleSystem::initParticleData(glm::vec2 startPoint, float stride, float maxWidth, bool randomize)
    {
        positionData.resize(particleCount);
        nextPositionData.resize(particleCount);
        velocityData.resize(particleCount);
        densityData.resize(particleCount);
        massData.resize(particleCount);

        spacialLookup.resize(particleCount);
        spacialLookupEntry.resize(particleCount);

        maxWidth -= std::fmod(maxWidth, stride);
        int cntPerRow = static_cast<int>(maxWidth / stride);
        int row, col;
        for (int i = 0; i < particleCount; i++)
        {
            row = static_cast<int>(i / cntPerRow);
            col = i % cntPerRow;

            if (randomize)
                positionData[i] = glm::vec2(
                    static_cast<float>(rand() % static_cast<int>(windowExtent.width)),
                    static_cast<float>(rand() % static_cast<int>(windowExtent.height)));
            else
                positionData[i] = startPoint + glm::vec2(col * stride, row * stride);

            velocityData[i] = glm::vec2(0.f, 0.f);

            massData[i] = 100.f;
        }
    }

    void FluidParticleSystem::updateParticleData(float deltaTime)
    {
        // predict position
        for (int i = 0; i < particleCount; i++)
            nextPositionData[i] = positionData[i] + velocityData[i] * lookAheadTime;

        updateSpatialLookup();

        // calculate density using predicted position
        for (int i = 0; i < particleCount; i++)
            densityData[i] = calculateDensity(nextPositionData[i]);

        // update velocity
        for (int i = 0; i < particleCount; i++)
        {
            glm::vec2 pressureForce = calculatePressureForce(i);
            glm::vec2 gravityAcc = glm::vec2(0.f, gravityAccValue * massData[i]);

            glm::vec2 acceleration = pressureForce / densityData[i] + gravityAcc;
            velocityData[i] += acceleration * deltaTime;
        }

        // update position
        for (int i = 0; i < particleCount; i++)
            positionData[i] += velocityData[i] * deltaTime;

        handleBoundaryCollision();
    }

    float FluidParticleSystem::kernelPoly6_2D(float distance, float radius) const
    {
        if (distance >= radius)
            return 0.f;
        float v = radius * radius - distance * distance;
        return scalingFactorPoly6_2D * v * v * v;
    }

    float FluidParticleSystem::kernelSpikyPow3_2D(float distance, float radius) const
    {
        if (distance >= radius)
            return 0.f;
        float v = radius - distance;
        return scalingFactorSpikyPow3_2D * v * v * v;
    }

    float FluidParticleSystem::derivativeSpikyPow3_2D(float distance, float radius) const
    {
        if (distance >= radius)
            return 0.f;
        float v = radius - distance;
        return -3.f * scalingFactorSpikyPow3_2D * v * v;
    }

    float FluidParticleSystem::kernelSpikyPow2_2D(float distance, float radius) const
    {
        if (distance >= radius)
            return 0.f;
        float v = radius - distance;
        return scalingFactorSpikyPow2_2D * v * v;
    }

    float FluidParticleSystem::derivativeSpikyPow2_2D(float distance, float radius) const
    {
        if (distance >= radius)
            return 0.f;
        float v = radius - distance;
        return -2.f * scalingFactorSpikyPow2_2D * v;
    }

    float FluidParticleSystem::calculateDensity(glm::vec2 samplePos)
    {
        float density = 0.f;
        for (int i = 0; i < particleCount; i++)
        {
            float distance = glm::distance(samplePos, positionData[i]);
            float influence = kernelPoly6_2D(distance, smoothRadius);
            density += massData[i] * influence;
        }
        return density;
    }

    glm::vec2 FluidParticleSystem::calculatePressureForce(unsigned int particleIndex)
    {
        glm::vec2 pressureForce = glm::vec2(0.f, 0.f);
        glm::vec2 particleNextPos = nextPositionData[particleIndex];
        float pressureThis = pressureMultiplier * (densityData[particleIndex] - targetDensity);

        foreachNeighbor(
            particleIndex,
            [&](int neighborIndex)
            {
                float distance = glm::distance(particleNextPos, nextPositionData[neighborIndex]);
                if (distance >= smoothRadius)
                    return;

                glm::vec2 dir;
                if (distance < glm::epsilon<float>())
                    dir = glm::circularRand(1.f);
                else
                    dir = glm::normalize(nextPositionData[neighborIndex] - particleNextPos);

                float pressureOther = pressureMultiplier * (densityData[neighborIndex] - targetDensity);
                float sharedPressure = (pressureThis + pressureOther) / 2.f;
                pressureForce += sharedPressure *
                                 derivativeSpikyPow2_2D(distance, smoothRadius) * massData[neighborIndex] /
                                 densityData[neighborIndex] * dir;
            });
        return pressureForce;
    }

    void FluidParticleSystem::handleBoundaryCollision()
    {
        for (int i = 0; i < particleCount; i++)
        {
            if (positionData[i].x < 0 || positionData[i].x > windowExtent.width)
            {
                positionData[i].x = std::clamp(positionData[i].x, 0.f, static_cast<float>(windowExtent.width));
                velocityData[i].x *= -collisionDamping;
            }
            if (positionData[i].y < 0 || positionData[i].y > windowExtent.height)
            {
                positionData[i].y = std::clamp(positionData[i].y, 0.f, static_cast<float>(windowExtent.height));
                velocityData[i].y *= -collisionDamping;
            }
        }
    }

    glm::int2 FluidParticleSystem::pos2gridCoord(glm::vec2 position, int gridWidth) const
    {
        int x = position.x / gridWidth;
        int y = position.y / gridWidth;
        return {x, y};
    }

    int FluidParticleSystem::hashGridCoord2D(glm::int2 gridCoord) const
    {
        return gridCoord.x * 73856093 ^ gridCoord.y * 83492791;
    }

    void FluidParticleSystem::updateSpatialLookup()
    {
        for (int i = 0; i < particleCount; i++)
        {
            int hashValue = hashGridCoord2D(pos2gridCoord(nextPositionData[i], static_cast<int>(smoothRadius)));
            unsigned int hashKey = LveMath::positiveMod(hashValue, particleCount);
            spacialLookup[i].particleIndex = i;
            spacialLookup[i].spatialHashKey = hashKey;
        }

        std::sort(
            spacialLookup.begin(),
            spacialLookup.end(),
            [](const SpatialHashEntry &a, const SpatialHashEntry &b)
            {
                return a.spatialHashKey < b.spatialHashKey;
            });

        for (int i = 0; i < particleCount; i++)
        {
            unsigned int key = spacialLookup[i].spatialHashKey;
            unsigned int keyPrev = (i == 0) ? -1 : spacialLookup[i - 1].spatialHashKey;
            if (key != keyPrev)
                spacialLookupEntry[key] = i;
        }
    }

    /*
     * Iterate over all neighbors of a particle, excluding itself
     * @param particleIndex: index of the particle
     * @param callback: function to be called for each neighbor
     */
    void FluidParticleSystem::foreachNeighbor(unsigned int particleIndex, std::function<void(int)> callback)
    {
        glm::vec2 particleNextPos = nextPositionData[particleIndex];
        glm::int2 gridPos = pos2gridCoord(particleNextPos, smoothRadius);
        for (int i = 0; i < 9; i++)
        {
            glm::int2 offsetGridPos = gridPos + offset2D[i];
            unsigned int hashKey = LveMath::positiveMod(hashGridCoord2D(offsetGridPos), particleCount);
            int startIndex = spacialLookupEntry[hashKey];

            for (int j = startIndex; j < particleCount; j++)
            {
                if (spacialLookup[j].spatialHashKey != hashKey)
                    break;

                unsigned int neighborIndex = spacialLookup[j].particleIndex;
                if (neighborIndex != particleIndex)
                    callback(neighborIndex);
            }
        }
    }
} // namespace lve