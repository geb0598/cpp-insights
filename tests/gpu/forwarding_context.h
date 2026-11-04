#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace insights {
namespace test {

// -------------------------------------------------
// ForwardingContext
// -------------------------------------------------
// An ID3D11DeviceContext that passes every call to a real one, so a test can override
// the few methods it wants to observe or alter. The methods mirror d3d11.h in declaration
// order; COM interfaces never change, so the list never needs updating. The test owns the
// object: AddRef and Release do not count, and QueryInterface hands out the real context.
class ForwardingContext : public ID3D11DeviceContext {
public:
    explicit ForwardingContext(ID3D11DeviceContext* inner) : inner_(inner) {}
    virtual ~ForwardingContext() = default;

    ForwardingContext(const ForwardingContext&)            = delete;
    ForwardingContext& operator=(const ForwardingContext&) = delete;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        return inner_->QueryInterface(riid, object);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return 1;
    }

    // ID3D11DeviceChild
    void STDMETHODCALLTYPE GetDevice(ID3D11Device** device) override {
        inner_->GetDevice(device);
    }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* data_size, void* data) override {
        return inner_->GetPrivateData(guid, data_size, data);
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT data_size, const void* data) override {
        return inner_->SetPrivateData(guid, data_size, data);
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* data) override {
        return inner_->SetPrivateDataInterface(guid, data);
    }

    // ID3D11DeviceContext
    void STDMETHODCALLTYPE VSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->VSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE PSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->PSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE PSSetShader(
        ID3D11PixelShader* pixel_shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->PSSetShader(pixel_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE PSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->PSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE VSSetShader(
        ID3D11VertexShader* vertex_shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->VSSetShader(vertex_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE DrawIndexed(
        UINT index_count, UINT start_index_location, INT base_vertex_location) override {
        inner_->DrawIndexed(index_count, start_index_location, base_vertex_location);
    }
    void STDMETHODCALLTYPE Draw(UINT vertex_count, UINT start_vertex_location) override {
        inner_->Draw(vertex_count, start_vertex_location);
    }
    HRESULT STDMETHODCALLTYPE Map(
        ID3D11Resource* resource, UINT subresource, D3D11_MAP map_type, UINT map_flags,
        D3D11_MAPPED_SUBRESOURCE* mapped_resource) override {
        return inner_->Map(resource, subresource, map_type, map_flags, mapped_resource);
    }
    void STDMETHODCALLTYPE Unmap(ID3D11Resource* resource, UINT subresource) override {
        inner_->Unmap(resource, subresource);
    }
    void STDMETHODCALLTYPE PSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->PSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout* input_layout) override {
        inner_->IASetInputLayout(input_layout);
    }
    void STDMETHODCALLTYPE IASetVertexBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* vertex_buffers, const UINT* strides,
        const UINT* offsets) override {
        inner_->IASetVertexBuffers(start_slot, num_buffers, vertex_buffers, strides, offsets);
    }
    void STDMETHODCALLTYPE IASetIndexBuffer(
        ID3D11Buffer* index_buffer, DXGI_FORMAT format, UINT offset) override {
        inner_->IASetIndexBuffer(index_buffer, format, offset);
    }
    void STDMETHODCALLTYPE DrawIndexedInstanced(
        UINT index_count_per_instance, UINT instance_count, UINT start_index_location,
        INT base_vertex_location, UINT start_instance_location) override {
        inner_->DrawIndexedInstanced(index_count_per_instance, instance_count, start_index_location,
                                     base_vertex_location, start_instance_location);
    }
    void STDMETHODCALLTYPE DrawInstanced(
        UINT vertex_count_per_instance, UINT instance_count, UINT start_vertex_location,
        UINT start_instance_location) override {
        inner_->DrawInstanced(vertex_count_per_instance, instance_count, start_vertex_location,
                              start_instance_location);
    }
    void STDMETHODCALLTYPE GSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->GSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE GSSetShader(
        ID3D11GeometryShader* shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->GSSetShader(shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology) override {
        inner_->IASetPrimitiveTopology(topology);
    }
    void STDMETHODCALLTYPE VSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->VSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE VSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->VSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE Begin(ID3D11Asynchronous* async) override {
        inner_->Begin(async);
    }
    void STDMETHODCALLTYPE End(ID3D11Asynchronous* async) override {
        inner_->End(async);
    }
    HRESULT STDMETHODCALLTYPE GetData(
        ID3D11Asynchronous* async, void* data, UINT data_size, UINT get_data_flags) override {
        return inner_->GetData(async, data, data_size, get_data_flags);
    }
    void STDMETHODCALLTYPE SetPredication(ID3D11Predicate* predicate, BOOL predicate_value) override {
        inner_->SetPredication(predicate, predicate_value);
    }
    void STDMETHODCALLTYPE GSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->GSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE GSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->GSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE OMSetRenderTargets(
        UINT num_views, ID3D11RenderTargetView* const* render_target_views,
        ID3D11DepthStencilView* depth_stencil_view) override {
        inner_->OMSetRenderTargets(num_views, render_target_views, depth_stencil_view);
    }
    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
        UINT num_rtvs, ID3D11RenderTargetView* const* render_target_views,
        ID3D11DepthStencilView* depth_stencil_view, UINT uav_start_slot, UINT num_uavs,
        ID3D11UnorderedAccessView* const* unordered_access_views, const UINT* uav_initial_counts) override {
        inner_->OMSetRenderTargetsAndUnorderedAccessViews(num_rtvs, render_target_views, depth_stencil_view,
                                                          uav_start_slot, num_uavs, unordered_access_views,
                                                          uav_initial_counts);
    }
    void STDMETHODCALLTYPE OMSetBlendState(
        ID3D11BlendState* blend_state, const FLOAT blend_factor[4], UINT sample_mask) override {
        inner_->OMSetBlendState(blend_state, blend_factor, sample_mask);
    }
    void STDMETHODCALLTYPE OMSetDepthStencilState(
        ID3D11DepthStencilState* depth_stencil_state, UINT stencil_ref) override {
        inner_->OMSetDepthStencilState(depth_stencil_state, stencil_ref);
    }
    void STDMETHODCALLTYPE SOSetTargets(
        UINT num_buffers, ID3D11Buffer* const* so_targets, const UINT* offsets) override {
        inner_->SOSetTargets(num_buffers, so_targets, offsets);
    }
    void STDMETHODCALLTYPE DrawAuto() override {
        inner_->DrawAuto();
    }
    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
        ID3D11Buffer* buffer_for_args, UINT aligned_byte_offset_for_args) override {
        inner_->DrawIndexedInstancedIndirect(buffer_for_args, aligned_byte_offset_for_args);
    }
    void STDMETHODCALLTYPE DrawInstancedIndirect(
        ID3D11Buffer* buffer_for_args, UINT aligned_byte_offset_for_args) override {
        inner_->DrawInstancedIndirect(buffer_for_args, aligned_byte_offset_for_args);
    }
    void STDMETHODCALLTYPE Dispatch(
        UINT thread_group_count_x, UINT thread_group_count_y, UINT thread_group_count_z) override {
        inner_->Dispatch(thread_group_count_x, thread_group_count_y, thread_group_count_z);
    }
    void STDMETHODCALLTYPE DispatchIndirect(
        ID3D11Buffer* buffer_for_args, UINT aligned_byte_offset_for_args) override {
        inner_->DispatchIndirect(buffer_for_args, aligned_byte_offset_for_args);
    }
    void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState* rasterizer_state) override {
        inner_->RSSetState(rasterizer_state);
    }
    void STDMETHODCALLTYPE RSSetViewports(UINT num_viewports, const D3D11_VIEWPORT* viewports) override {
        inner_->RSSetViewports(num_viewports, viewports);
    }
    void STDMETHODCALLTYPE RSSetScissorRects(UINT num_rects, const D3D11_RECT* rects) override {
        inner_->RSSetScissorRects(num_rects, rects);
    }
    void STDMETHODCALLTYPE CopySubresourceRegion(
        ID3D11Resource* dst_resource, UINT dst_subresource, UINT dst_x, UINT dst_y, UINT dst_z,
        ID3D11Resource* src_resource, UINT src_subresource, const D3D11_BOX* src_box) override {
        inner_->CopySubresourceRegion(dst_resource, dst_subresource, dst_x, dst_y, dst_z, src_resource,
                                      src_subresource, src_box);
    }
    void STDMETHODCALLTYPE CopyResource(ID3D11Resource* dst_resource, ID3D11Resource* src_resource) override {
        inner_->CopyResource(dst_resource, src_resource);
    }
    void STDMETHODCALLTYPE UpdateSubresource(
        ID3D11Resource* dst_resource, UINT dst_subresource, const D3D11_BOX* dst_box, const void* src_data,
        UINT src_row_pitch, UINT src_depth_pitch) override {
        inner_->UpdateSubresource(dst_resource, dst_subresource, dst_box, src_data, src_row_pitch,
                                  src_depth_pitch);
    }
    void STDMETHODCALLTYPE CopyStructureCount(
        ID3D11Buffer* dst_buffer, UINT dst_aligned_byte_offset,
        ID3D11UnorderedAccessView* src_view) override {
        inner_->CopyStructureCount(dst_buffer, dst_aligned_byte_offset, src_view);
    }
    void STDMETHODCALLTYPE ClearRenderTargetView(
        ID3D11RenderTargetView* render_target_view, const FLOAT color_rgba[4]) override {
        inner_->ClearRenderTargetView(render_target_view, color_rgba);
    }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
        ID3D11UnorderedAccessView* unordered_access_view, const UINT values[4]) override {
        inner_->ClearUnorderedAccessViewUint(unordered_access_view, values);
    }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
        ID3D11UnorderedAccessView* unordered_access_view, const FLOAT values[4]) override {
        inner_->ClearUnorderedAccessViewFloat(unordered_access_view, values);
    }
    void STDMETHODCALLTYPE ClearDepthStencilView(
        ID3D11DepthStencilView* depth_stencil_view, UINT clear_flags, FLOAT depth, UINT8 stencil) override {
        inner_->ClearDepthStencilView(depth_stencil_view, clear_flags, depth, stencil);
    }
    void STDMETHODCALLTYPE GenerateMips(ID3D11ShaderResourceView* shader_resource_view) override {
        inner_->GenerateMips(shader_resource_view);
    }
    void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource* resource, FLOAT min_lod) override {
        inner_->SetResourceMinLOD(resource, min_lod);
    }
    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource* resource) override {
        return inner_->GetResourceMinLOD(resource);
    }
    void STDMETHODCALLTYPE ResolveSubresource(
        ID3D11Resource* dst_resource, UINT dst_subresource, ID3D11Resource* src_resource,
        UINT src_subresource, DXGI_FORMAT format) override {
        inner_->ResolveSubresource(dst_resource, dst_subresource, src_resource, src_subresource, format);
    }
    void STDMETHODCALLTYPE ExecuteCommandList(
        ID3D11CommandList* command_list, BOOL restore_context_state) override {
        inner_->ExecuteCommandList(command_list, restore_context_state);
    }
    void STDMETHODCALLTYPE HSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->HSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE HSSetShader(
        ID3D11HullShader* hull_shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->HSSetShader(hull_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE HSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->HSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE HSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->HSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE DSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->DSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE DSSetShader(
        ID3D11DomainShader* domain_shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->DSSetShader(domain_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE DSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->DSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE DSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->DSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE CSSetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView* const* shader_resource_views) override {
        inner_->CSSetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
        UINT start_slot, UINT num_uavs, ID3D11UnorderedAccessView* const* unordered_access_views,
        const UINT* uav_initial_counts) override {
        inner_->CSSetUnorderedAccessViews(start_slot, num_uavs, unordered_access_views, uav_initial_counts);
    }
    void STDMETHODCALLTYPE CSSetShader(
        ID3D11ComputeShader* compute_shader, ID3D11ClassInstance* const* class_instances,
        UINT num_class_instances) override {
        inner_->CSSetShader(compute_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE CSSetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState* const* samplers) override {
        inner_->CSSetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE CSSetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer* const* constant_buffers) override {
        inner_->CSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE VSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->VSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE PSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->PSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE PSGetShader(
        ID3D11PixelShader** pixel_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->PSGetShader(pixel_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE PSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->PSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE VSGetShader(
        ID3D11VertexShader** vertex_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->VSGetShader(vertex_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE PSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->PSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout** input_layout) override {
        inner_->IAGetInputLayout(input_layout);
    }
    void STDMETHODCALLTYPE IAGetVertexBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** vertex_buffers, UINT* strides,
        UINT* offsets) override {
        inner_->IAGetVertexBuffers(start_slot, num_buffers, vertex_buffers, strides, offsets);
    }
    void STDMETHODCALLTYPE IAGetIndexBuffer(
        ID3D11Buffer** index_buffer, DXGI_FORMAT* format, UINT* offset) override {
        inner_->IAGetIndexBuffer(index_buffer, format, offset);
    }
    void STDMETHODCALLTYPE GSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->GSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE GSGetShader(
        ID3D11GeometryShader** geometry_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->GSGetShader(geometry_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* topology) override {
        inner_->IAGetPrimitiveTopology(topology);
    }
    void STDMETHODCALLTYPE VSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->VSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE VSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->VSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE GetPredication(ID3D11Predicate** predicate, BOOL* predicate_value) override {
        inner_->GetPredication(predicate, predicate_value);
    }
    void STDMETHODCALLTYPE GSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->GSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE GSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->GSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE OMGetRenderTargets(
        UINT num_views, ID3D11RenderTargetView** render_target_views,
        ID3D11DepthStencilView** depth_stencil_view) override {
        inner_->OMGetRenderTargets(num_views, render_target_views, depth_stencil_view);
    }
    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
        UINT num_rtvs, ID3D11RenderTargetView** render_target_views,
        ID3D11DepthStencilView** depth_stencil_view, UINT uav_start_slot, UINT num_uavs,
        ID3D11UnorderedAccessView** unordered_access_views) override {
        inner_->OMGetRenderTargetsAndUnorderedAccessViews(num_rtvs, render_target_views, depth_stencil_view,
                                                          uav_start_slot, num_uavs, unordered_access_views);
    }
    void STDMETHODCALLTYPE OMGetBlendState(
        ID3D11BlendState** blend_state, FLOAT blend_factor[4], UINT* sample_mask) override {
        inner_->OMGetBlendState(blend_state, blend_factor, sample_mask);
    }
    void STDMETHODCALLTYPE OMGetDepthStencilState(
        ID3D11DepthStencilState** depth_stencil_state, UINT* stencil_ref) override {
        inner_->OMGetDepthStencilState(depth_stencil_state, stencil_ref);
    }
    void STDMETHODCALLTYPE SOGetTargets(UINT num_buffers, ID3D11Buffer** so_targets) override {
        inner_->SOGetTargets(num_buffers, so_targets);
    }
    void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState** rasterizer_state) override {
        inner_->RSGetState(rasterizer_state);
    }
    void STDMETHODCALLTYPE RSGetViewports(UINT* num_viewports, D3D11_VIEWPORT* viewports) override {
        inner_->RSGetViewports(num_viewports, viewports);
    }
    void STDMETHODCALLTYPE RSGetScissorRects(UINT* num_rects, D3D11_RECT* rects) override {
        inner_->RSGetScissorRects(num_rects, rects);
    }
    void STDMETHODCALLTYPE HSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->HSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE HSGetShader(
        ID3D11HullShader** hull_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->HSGetShader(hull_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE HSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->HSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE HSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->HSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE DSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->DSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE DSGetShader(
        ID3D11DomainShader** domain_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->DSGetShader(domain_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE DSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->DSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE DSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->DSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE CSGetShaderResources(
        UINT start_slot, UINT num_views, ID3D11ShaderResourceView** shader_resource_views) override {
        inner_->CSGetShaderResources(start_slot, num_views, shader_resource_views);
    }
    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
        UINT start_slot, UINT num_uavs, ID3D11UnorderedAccessView** unordered_access_views) override {
        inner_->CSGetUnorderedAccessViews(start_slot, num_uavs, unordered_access_views);
    }
    void STDMETHODCALLTYPE CSGetShader(
        ID3D11ComputeShader** compute_shader, ID3D11ClassInstance** class_instances,
        UINT* num_class_instances) override {
        inner_->CSGetShader(compute_shader, class_instances, num_class_instances);
    }
    void STDMETHODCALLTYPE CSGetSamplers(
        UINT start_slot, UINT num_samplers, ID3D11SamplerState** samplers) override {
        inner_->CSGetSamplers(start_slot, num_samplers, samplers);
    }
    void STDMETHODCALLTYPE CSGetConstantBuffers(
        UINT start_slot, UINT num_buffers, ID3D11Buffer** constant_buffers) override {
        inner_->CSGetConstantBuffers(start_slot, num_buffers, constant_buffers);
    }
    void STDMETHODCALLTYPE ClearState() override {
        inner_->ClearState();
    }
    void STDMETHODCALLTYPE Flush() override {
        inner_->Flush();
    }
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() override {
        return inner_->GetType();
    }
    UINT STDMETHODCALLTYPE GetContextFlags() override {
        return inner_->GetContextFlags();
    }
    HRESULT STDMETHODCALLTYPE FinishCommandList(
        BOOL restore_deferred_context_state, ID3D11CommandList** command_list) override {
        return inner_->FinishCommandList(restore_deferred_context_state, command_list);
    }

protected:
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> inner_;
};

} // namespace test
} // namespace insights
