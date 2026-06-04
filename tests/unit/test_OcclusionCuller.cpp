#include <gtest/gtest.h>
#include "Renderer/OcclusionCuller.h"
#include "Scene/SceneNode.h"

using namespace RRE;

// ---- Helpers ---------------------------------------------------------------

// Build a small array of dummy (non-null) SceneNode pointers via stack allocation.
// We only need stable addresses; the nodes are never traversed here.
static SceneNode s_nodePool[16];

// ---- Tests -----------------------------------------------------------------

TEST(OcclusionCuller, DefaultState_NeverOccluded)
{
    OcclusionCuller culler;

    // With no results loaded, every node must report visible (conservative default).
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[0]));
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[1]));
    EXPECT_FALSE(culler.IsOccluded(nullptr));
}

TEST(OcclusionCuller, UpdateResults_MarksOccludedNodes)
{
    OcclusionCuller culler;

    const SceneNode* nodes[3] = { &s_nodePool[0], &s_nodePool[1], &s_nodePool[2] };
    uint32 results[3]         = { 0u, 1u, 0u };  // [visible, occluded, visible]

    culler.UpdateResults(nodes, results, 3);

    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[0]));  // result=0 → visible
    EXPECT_TRUE (culler.IsOccluded(&s_nodePool[1]));  // result=1 → occluded
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[2]));  // result=0 → visible
}

TEST(OcclusionCuller, UnknownNode_DefaultsToVisible)
{
    OcclusionCuller culler;

    const SceneNode* nodes[2] = { &s_nodePool[0], &s_nodePool[1] };
    uint32 results[2]         = { 1u, 1u };  // both occluded

    culler.UpdateResults(nodes, results, 2);

    // s_nodePool[2] was never added — must conservatively say visible.
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[2]));
}

TEST(OcclusionCuller, ClearResults_RestoresVisibility)
{
    OcclusionCuller culler;

    const SceneNode* nodes[2] = { &s_nodePool[0], &s_nodePool[1] };
    uint32 results[2]         = { 1u, 1u };

    culler.UpdateResults(nodes, results, 2);
    EXPECT_TRUE(culler.IsOccluded(&s_nodePool[0]));

    culler.ClearResults();

    // After clearing, nodes that were occluded must return visible.
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[0]));
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[1]));
}

TEST(OcclusionCuller, UpdateResults_ReplacesOldState)
{
    OcclusionCuller culler;

    const SceneNode* nodes[2] = { &s_nodePool[0], &s_nodePool[1] };

    // First update: both occluded
    uint32 results1[2] = { 1u, 1u };
    culler.UpdateResults(nodes, results1, 2);
    EXPECT_TRUE(culler.IsOccluded(&s_nodePool[0]));
    EXPECT_TRUE(culler.IsOccluded(&s_nodePool[1]));

    // Second update: both visible — must replace previous state
    uint32 results2[2] = { 0u, 0u };
    culler.UpdateResults(nodes, results2, 2);
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[0]));
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[1]));
}

TEST(OcclusionCuller, UpdateResults_CountZero_NoChange)
{
    OcclusionCuller culler;

    // Pre-populate with one occluded node
    const SceneNode* nodes[1] = { &s_nodePool[0] };
    uint32 results[1]         = { 1u };
    culler.UpdateResults(nodes, results, 1);
    EXPECT_TRUE(culler.IsOccluded(&s_nodePool[0]));

    // Update with count=0 — should clear all (UpdateResults always clears first)
    culler.UpdateResults(nullptr, nullptr, 0);
    EXPECT_FALSE(culler.IsOccluded(&s_nodePool[0]));
}

TEST(OcclusionCuller, UpdateResults_AllOccluded)
{
    OcclusionCuller culler;

    const int N = 8;
    const SceneNode* nodes[N];
    uint32 results[N];
    for (int i = 0; i < N; i++)
    {
        nodes[i]   = &s_nodePool[i];
        results[i] = 1u;  // all occluded
    }

    culler.UpdateResults(nodes, results, N);

    for (int i = 0; i < N; i++)
        EXPECT_TRUE(culler.IsOccluded(&s_nodePool[i]));
}

TEST(OcclusionCuller, UpdateResults_AllVisible)
{
    OcclusionCuller culler;

    const int N = 8;
    const SceneNode* nodes[N];
    uint32 results[N];
    for (int i = 0; i < N; i++)
    {
        nodes[i]   = &s_nodePool[i];
        results[i] = 0u;  // all visible
    }

    culler.UpdateResults(nodes, results, N);

    for (int i = 0; i < N; i++)
        EXPECT_FALSE(culler.IsOccluded(&s_nodePool[i]));
}

TEST(OcclusionCuller, SameNodeUpdatedTwiceInOneCall_LastResultWins)
{
    // GPU results may contain duplicate node pointers if the same mesh
    // appears in multiple draw calls. The last entry in the array should win.
    OcclusionCuller culler;

    // Simulate: node[0] appears twice — first visible, then occluded
    const SceneNode* nodes[2] = { &s_nodePool[0], &s_nodePool[0] };
    uint32 results[2]         = { 0u, 1u };

    culler.UpdateResults(nodes, results, 2);

    // map insertion order: second write (1=occluded) overwrites first (0=visible)
    EXPECT_TRUE(culler.IsOccluded(&s_nodePool[0]));
}
