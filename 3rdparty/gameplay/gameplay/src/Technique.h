#pragma once

#include "Pass.h"


namespace gameplay
{
    class Material;


    /**
     * Defines a technique for how an object to be rendered.
     *
     * You should use techniques to support different sets of
     * shader + definitions that will be all loaded when a material
     * is loaded. You can then change the rendering technique at
     * runtime without having to first load the shaders.
     *
     * A technique has one or more passes for supporting multi pass rendering.
     */
    class Technique : public RenderState
    {
        friend class Material;
        friend class Pass;
        friend class RenderState;

    public:
        Technique(const char* id, const std::shared_ptr<Material>& material);
        ~Technique();

        /**
         * Gets the id of this technique.
         *
         * @return The Id of this technique.
         */
        const char* getId() const;

        /**
         * Gets the number of passes in this technique.
         *
         * @return The number of passes in this technique.
         */
        size_t getPassCount() const;

        /**
         * Gets the pass at the specified index.
         *
         * @return The pass at the specified index.
         */
        const std::shared_ptr<Pass>& getPassByIndex(size_t index) const;

        /**
         * Gets the pass with the specified id.
         *
         * @return The pass at the specified id.
         */
        std::shared_ptr<Pass> getPass(const char* id) const;

        /**
         * @see RenderState::setNodeBinding
         */
        void setNodeBinding(Node* node) override;

    private:

        Technique(const Technique&) = delete;

        Technique& operator=(const Technique&) = delete;

        std::string _id;
        std::shared_ptr<Material> _material;
        std::vector<std::shared_ptr<Pass>> _passes;
    };
}
