#pragma once

#include "Node.h"
#include "Light.h"
#include "Model.h"


namespace gameplay
{
    /**
     * Defines the root container for a hierarchy of Node objects.
     *
     * @see http://gameplay3d.github.io/GamePlay/docs/file-formats.html#wiki-Scene
     */
    class Scene
    {
    public:

        /**
         * Creates a new empty scene.
         *
         * @param id ID of the new scene, or nullptr to use an empty string for the ID (default).
         *
         * @return The newly created empty scene.
         * @script{create}
         */
        static Scene* create(const char* id = nullptr);

        /**
         * Gets a currently active scene.
         *
         * If id is an nullptr, the first active scene is returned.
         *
         * @param id ID of the scene to retrieve, or nullptr to retrieve the first active scene.
         *
         * @return The scene that matches the specified ID, or nullptr if no matching scene could be found.
         */
        static Scene* getScene(const char* id = nullptr);

        /**
         * Gets the identifier for the scene.
         *
         * @return The scene identifier.
         */
        const char* getId() const;

        /**
         * Sets the identifier for the scene.
         *
         * @param id The identifier to set for the scene.
         */
        void setId(const char* id);

        /**
         * Returns the first node in the scene that matches the given ID.
         *
         * @param id The ID of the node to find.
         * @param recursive true if a recursive search should be performed, false otherwise.
         * @param exactMatch true if only nodes whose ID exactly matches the specified ID are returned,
         *      or false if nodes that start with the given ID are returned.
         *
         * @return The first node found that matches the given ID.
         */
        std::shared_ptr<Node> findNode(const char* id, bool recursive = true, bool exactMatch = true) const;

        /**
         * Returns all nodes in the scene that match the given ID.
         *
         * @param id The ID of the node to find.
         * @param nodes Vector of nodes to be populated with matches.
         * @param recursive true if a recursive search should be performed, false otherwise.
         * @param exactMatch true if only nodes who's ID exactly matches the specified ID are returned,
         *      or false if nodes that start with the given ID are returned.
         *
         * @return The number of matches found.
         * @script{ignore}
         */
        size_t findNodes(const char* id, Node::List& nodes, bool recursive = true, bool exactMatch = true) const;

        /**
         * Creates and adds a new node to the scene.
         *
         * @param id An optional node ID.
         *
         * @return The new node.
         */
        std::shared_ptr<Node> addNode(const std::string& id);

        /**
         * Adds the specified node to the scene.
         *
         * @param node The node to be added to the scene.
         */
        void addNode(const std::shared_ptr<Node>& node);

        /**
         * Removes the specified node from the scene.
         *
         * @param node The node to remove.
         */
        void removeNode(const std::shared_ptr<Node>& node);

        /**
         * Removes all nodes from the scene.
         */
        void removeAllNodes();

        /**
         * Returns the number of nodes at the root level of the scene.
         *
         * @return The node count.
         */
        size_t getNodeCount() const;

        /**
         * Gets the active camera for the scene.
         *
         * @return The active camera for the scene.
         * @see VisibleSet#getActiveCamera
         */
        const std::shared_ptr<Camera>& getActiveCamera() const;

        /**
         * Sets the active camera on the scene.
         *
         * @param camera The active camera to be set on the scene.
         */
        void setActiveCamera(const std::shared_ptr<Camera>& camera);

        /**
         * Returns the ambient color of the scene.
         *
         * The default ambient light color is black (0,0,0).
         *
         * This value can be bound to materials using the SCENE_LIGHT_AMBIENT_COLOR auto binding.
         *
         * @return The scene's ambient color.
         */
        const Vector3& getAmbientColor() const;

        /**
         * Sets the ambient color of the scene.
         *
         * @param red The red channel between 0.0 and 1.0.
         * @param green The green channel between 0.0 and 1.0.
         * @param blue The blue channel between 0.0 and 1.0.
         *
         * @see getAmbientColor()
         */
        void setAmbientColor(float red, float green, float blue);

        /**
         * Updates all active nodes in the scene.
         *
         * This method is recursively calls the Node::update(float) method on all nodes that
         * are active within the scene. A Node is considered active if Node::isActive()
         * returns true.
         *
         * @param elapsedTime Elapsed time in milliseconds.
         */
        void update(float elapsedTime);

        /**
         * Visits each node in the scene and calls the specified method pointer.
         *
         * Calling this method invokes the specified method pointer for each node
         * in the scene hierarchy.
         *
         * The visitMethod parameter must be a pointer to a method that has a bool
         * return type and accepts a single parameter of type Node*.
         *
         * A depth-first traversal of the scene continues while the visit method
         * returns true. Returning false will stop traversing further children for
         * the given node and the traversal will continue at the next sibling.
         *
         * @param instance The pointer to an instance of the object that contains visitMethod.
         * @param visitMethod The pointer to the class method to call for each node in the scene.
         */
        template<class T>
        void visit(T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&));

        /**
         * Visits each node in the scene and calls the specified method pointer.
         *
         * Calling this method invokes the specified method pointer for each node
         * in the scene hierarchy, passing the Node and the specified cookie value.
         *
         * The visitMethod parameter must be a pointer to a method that has a bool
         * return type and accepts two parameters: a Node pointer and a cookie of a
         * user-specified type.
         *
         * A depth-first traversal of the scene continues while the visit method
         * returns true. Returning false will stop traversing further children for
         * the given node and the traversal will continue at the next sibling.
         *
         * @param instance The pointer to an instance of the object that contains visitMethod.
         * @param visitMethod The pointer to the class method to call for each node in the scene.
         * @param cookie An optional user-defined parameter that will be passed to each invocation of visitMethod.
         */
        template<class T, class C>
        void visit(T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&, C), C cookie);

        /**
         * Visits each node in the scene and calls the specified Lua function.
         *
         * Calling this method invokes the specified Lua function for each node
         * in the scene hierarchy.
         *
         * The visitMethod parameter must be a string containing the name of a
         * valid Lua function that has a boolean return type and accepts a
         * single parameter of type Node*.
         *
         * A depth-first traversal of the scene continues while the visit method
         * returns true. Returning false will stop traversing further children for
         * the given node and the traversal will continue at the next sibling.
         *
         * @param visitMethod The name of the Lua function to call for each node in the scene.
         */
        inline void visit(const char* visitMethod);

    private:

        /**
         * Constructor.
         */
        Scene();

        /**
         * Hidden copy constructor.
         */
        Scene(const Scene& copy) = delete;

        /**
         * Destructor.
         */
        virtual ~Scene();

        Scene& operator=(const Scene&) = delete;

        /**
         * Visits the given node and all of its children recursively.
         */
        template<class T>
        void visitNode(const std::shared_ptr<Node>& node, T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&));

        /**
         * Visits the given node and all of its children recursively.
         */
        template<class T, class C>
        void visitNode(const std::shared_ptr<Node>& node, T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&, C), C cookie);

        /**
         * Visits the given node and all of its children recursively.
         */
        void visitNode(const std::shared_ptr<Node>& node, const char* visitMethod) const;

        bool isNodeVisible(const std::shared_ptr<Node>& node) const;

        std::string _id;
        std::shared_ptr<Camera> _activeCamera;
        Node::List _nodes;
        Vector3 _ambientColor;
    };


    template<class T>
    void Scene::visit(T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&))
    {
        for( const auto& node : _nodes )
        {
            visitNode(node, instance, visitMethod);
        }
    }


    template<class T, class C>
    void Scene::visit(T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&, C), C cookie)
    {
        for( const auto& node : _nodes )
        {
            visitNode(node, instance, visitMethod, cookie);
        }
    }


    inline void Scene::visit(const char* visitMethod)
    {
        for( const auto& node : _nodes )
        {
            visitNode(node, visitMethod);
        }
    }


    template<class T>
    void Scene::visitNode(const std::shared_ptr<Node>& node, T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&))
    {
        // Invoke the visit method for this node.
        if( !(instance ->* visitMethod)(node) )
            return;

        // If this node has a model with a mesh skin, visit the joint hierarchy within it
        // since we don't add joint hierarchies directly to the scene. If joints are never
        // visited, it's possible that nodes embedded within the joint hierarchy that contain
        // models will never get visited (and therefore never get drawn).
        auto model = std::dynamic_pointer_cast<Model>(node->getDrawable());
        if( model && model->_skin && model->_skin->_rootNode )
        {
            visitNode(model->_skin->_rootNode, instance, visitMethod);
        }

        // Recurse for all children.
        for( const auto& child : node->getChildren() )
        {
            visitNode(child, instance, visitMethod);
        }
    }


    template<class T, class C>
    void Scene::visitNode(const std::shared_ptr<Node>& node, T* instance, bool (T::*visitMethod)(const std::shared_ptr<Node>&, C), C cookie)
    {
        // Invoke the visit method for this node.
        if( !(instance ->* visitMethod)(node, cookie) )
            return;

        // If this node has a model with a mesh skin, visit the joint hierarchy within it
        // since we don't add joint hierarchies directly to the scene. If joints are never
        // visited, it's possible that nodes embedded within the joint hierarchy that contain
        // models will never get visited (and therefore never get drawn).
        auto model = std::dynamic_pointer_cast<Model>(node->getDrawable());
        if( model && model->_skin && model->_skin->_rootNode )
        {
            visitNode(model->_skin->_rootNode, instance, visitMethod, cookie);
        }

        // Recurse for all children.
        for( const auto& child : node->getChildren() )
        {
            visitNode(child, instance, visitMethod, cookie);
        }
    }
}