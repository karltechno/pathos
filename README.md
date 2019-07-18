# pathos

Work in progress hobby graphics framework. Main goals are to learn more about explicit API's (D3D12 currently) and experiment with various rendering techniques. 

Currently implemented:
* Standard GGX/Lambert PBR rendering based on Frostbite white paper.
* Standard environment map irradiance and specular (GGX split-sum) baking.
* GLTF model loading (not complete).
* Work in progress D3D12 backend.
* CVar implementation. 
* Stable cascaded shadow maps (currently with no filtering).

Things to do:
* Compressed textures (Basis?).
* Light culling (tiled/clustered).
* Experiment with GPU driven rendering (cluster/triangle culling, HI-Z culling, bindless, lod selection etc). In particular many of the techniques discussed [here](https://www.gdcvault.com/play/1023109/Optimizing-the-Graphics-Pipeline-With).
* Shadow map filtering.
* Experiment with wave ops (only supporting SM6/DXC anyway).
* Vulkan... eventually.
* Endless other things...
